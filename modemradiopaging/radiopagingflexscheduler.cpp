///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include "radiopagingflexscheduler.h"

#include <algorithm>

const int RadioPagingFlexScheduler::m_framesPerCycle;
const int RadioPagingFlexScheduler::m_cyclesPerHour;
const int RadioPagingFlexScheduler::m_framesPerHour;
const int RadioPagingFlexScheduler::m_protectedFrames;
const size_t RadioPagingFlexScheduler::m_numberCounterMax;

namespace {

const int framesPerCycle = RadioPagingFlexScheduler::m_framesPerCycle;
const int maxCandidates = 256;

} // namespace

// ---------------------------------------------------------------------------
// Message number counter
// ---------------------------------------------------------------------------

void RadioPagingFlexScheduler::NumberCounter::touch(List::iterator it)
{
    m_order.splice(m_order.begin(), m_order, it);
}

void RadioPagingFlexScheduler::NumberCounter::insert(uint64_t capcode, uint8_t value)
{
    while (!m_order.empty() && (m_order.size() >= m_maxEntries))
    {
        m_index.erase(m_order.back().first);
        m_order.pop_back();
    }

    m_order.push_front(std::make_pair(capcode, value));
    m_index[capcode] = m_order.begin();
}

uint8_t RadioPagingFlexScheduler::NumberCounter::upcoming(uint64_t capcode, int maxNumber) const
{
    auto found = m_index.find(capcode);

    if (found == m_index.end()) {
        return 0;
    }

    int value = found->second->second;
    return (value >= std::max(0, std::min(63, maxNumber))) ? 0 : (uint8_t) (value + 1);
}

uint8_t RadioPagingFlexScheduler::NumberCounter::next(uint64_t capcode, int maxNumber)
{
    uint8_t value = upcoming(capcode, maxNumber);
    auto found = m_index.find(capcode);

    if (found != m_index.end())
    {
        List::iterator it = found->second;
        touch(it);
        it->second = value;
        return value;
    }

    insert(capcode, value);
    return value;
}

int RadioPagingFlexScheduler::NumberCounter::peek(uint64_t capcode) const
{
    auto found = m_index.find(capcode);
    return (found == m_index.end()) ? -1 : found->second->second;
}

void RadioPagingFlexScheduler::NumberCounter::set(uint64_t capcode, uint8_t value)
{
    auto found = m_index.find(capcode);

    if (found != m_index.end())
    {
        touch(found->second);
        found->second->second = value & 0x3f;
        return;
    }

    insert(capcode, value & 0x3f);
}

// ---------------------------------------------------------------------------
// Time grid
// ---------------------------------------------------------------------------

RadioPagingFlexScheduler::CapcodeSlot RadioPagingFlexScheduler::capcodeSlot(uint64_t capcode)
{
    CapcodeSlot slot;
    slot.m_frame = (uint32_t) ((capcode / 16) % framesPerCycle);
    slot.m_phase = (uint32_t) ((capcode / 4) % 4);
    return slot;
}

RadioPagingFlexScheduler::FrameTime RadioPagingFlexScheduler::frameTime(uint64_t frameIndex)
{
    FrameTime time;
    time.m_frame = (uint32_t) (frameIndex % framesPerCycle);
    time.m_cycle = (uint32_t) ((frameIndex / framesPerCycle) % m_cyclesPerHour);
    time.m_absolute = time.m_cycle * framesPerCycle + time.m_frame;
    return time;
}

uint32_t RadioPagingFlexScheduler::nextFrame(uint32_t current, uint32_t assigned, int collapse)
{
    if (collapse <= 0) {
        return current;
    }

    uint32_t period = 1u << collapse;
    uint32_t target = assigned % period;
    uint32_t remainder = current % period;

    if (remainder <= target) {
        return current + (target - remainder);
    }

    return current + (period - remainder + target);
}

bool RadioPagingFlexScheduler::frameEligible(uint64_t capcode, uint32_t frame, int collapse)
{
    return nextFrame(frame, capcodeSlot(capcode).m_frame, collapse) == frame;
}

// ---------------------------------------------------------------------------
// Encoder
// ---------------------------------------------------------------------------

RadioPagingFlexEncoder::RadioPagingFlexEncoder() :
    m_mode(RadioPagingFlex::Mode1600_2),
    m_inverted(false),
    m_collapse(0),
    m_reserved(RadioPagingFlexScheduler::m_framesPerCycle, 0),
    m_nextId(1)
{
}

bool RadioPagingFlexEncoder::setCollapse(int collapse, std::string& error)
{
    if ((collapse < 0) || (collapse > RadioPagingFlex::m_maxCollapse))
    {
        error = "Collapse must be 0 to 7";
        return false;
    }

    m_collapse = collapse;
    return true;
}

bool RadioPagingFlexEncoder::setReservedFrames(const std::vector<uint8_t>& table, std::string& error)
{
    if (table.empty())
    {
        m_reserved.assign(RadioPagingFlexScheduler::m_framesPerCycle, 0);
        return true;
    }
    if (table.size() != (size_t) RadioPagingFlexScheduler::m_framesPerCycle)
    {
        error = "Reserved frame table must have 128 entries";
        return false;
    }

    for (int f = 0; f < RadioPagingFlexScheduler::m_protectedFrames; f++)
    {
        if (table[f])
        {
            error = "Frames 0 to 3 can not be reserved";
            return false;
        }
    }

    m_reserved = table;
    for (uint8_t& r : m_reserved) {
        r = r ? 1 : 0;
    }
    return true;
}

bool RadioPagingFlexEncoder::setNumbering(const Numbering& numbering, std::string& error)
{
    if ((numbering.m_maxNumber < 0) || (numbering.m_maxNumber > 63))
    {
        error = "Largest message number must be 0 to 63";
        return false;
    }

    m_numbering = numbering;
    return true;
}

int RadioPagingFlexEncoder::freeNumber(uint64_t capcode, int avoid) const
{
    bool used[64] = {false};

    if ((avoid >= 0) && (avoid < 64)) {
        used[avoid] = true;
    }

    // Messages of the address in progress: numbered and still queued (fragments to go, or placed in the frame
    // being built)
    for (const Item& other : m_queue)
    {
        if ((other.m_message.m_capcode == capcode) && (other.m_number >= 0)) {
            used[other.m_number & 0x3f] = true;
        }
    }

    for (int n = 0; n < 64; n++)
    {
        if (!used[n]) {
            return n;
        }
    }

    return 0;
}

void RadioPagingFlexEncoder::assignNumber(Item& item)
{
    const TxMessage& m = item.m_message;
    const uint64_t capcode = m.m_capcode;

    // A group message to a temporary address has no message number (STD-43A 5.2)
    if (RadioPagingFlex::isTemporaryAddress(capcode))
    {
        item.m_number = 0;
        item.m_retrieval = false;
        return;
    }
    const bool numbered = (m.m_type == RadioPagingFlex::Alphanumeric) || (m.m_type == RadioPagingFlex::Binary)
        || ((m.m_type == RadioPagingFlex::Numeric) && RadioPagingFlex::isNumberedFormat(m.m_numericFormat))
        || ((m.m_type == RadioPagingFlex::ShortMessage) && (m.m_shortType == RadioPagingFlex::ShortNumbered));
    int number = 0;
    bool retrieval = false;
    // The data message after a header message has the number of the header (3.10.1.2 H)
    auto header = ((m.m_type == RadioPagingFlex::Binary) && !m.m_binary.m_header) ? m_headerNumbers.find(capcode)
        : m_headerNumbers.end();

    if (header != m_headerNumbers.end())
    {
        number = header->second;
        retrieval = m_numbering.m_enabled;
        m_headerNumbers.erase(header);
    }
    else if (numbered && m_numbering.m_enabled)
    {
        number = m_numbers.next(capcode, m_numbering.m_maxNumber);
        retrieval = true;
    }
    else if ((m.m_type == RadioPagingFlex::Secure) || numbered)
    {
        // Not numbered: a number that tells the message apart from the others of the address in progress
        // (3.10.1.2-3.10.1.4 N), not the next one of the sequence. 0 when the address has none in progress
        int avoid = m_numbering.m_enabled ? (int) m_numbers.upcoming(capcode, m_numbering.m_maxNumber) : -1;
        number = freeNumber(capcode, avoid);
    }

    if ((m.m_type == RadioPagingFlex::Binary) && m.m_binary.m_header) {
        m_headerNumbers[capcode] = number;
    }

    // The fragments of a message have the number of its first fragment (N, 3.10.1.2 p.60)
    for (Item& other : m_queue)
    {
        if ((other.m_id == item.m_id) && (other.m_number < 0))
        {
            other.m_number = number;
            other.m_retrieval = retrieval;
        }
    }

    item.m_number = number;
    item.m_retrieval = retrieval;
}

bool RadioPagingFlexEncoder::schedulable(uint64_t capcode) const
{
    for (int f = 0; f < RadioPagingFlexScheduler::m_framesPerCycle; f++)
    {
        if (!m_reserved[f] && RadioPagingFlexScheduler::frameEligible(capcode, f, m_collapse)) {
            return true;
        }
    }

    return false;
}

RadioPagingFlex::PhaseMessage RadioPagingFlexEncoder::phaseMessage(const Item& item)
{
    RadioPagingFlex::PhaseMessage pm;
    const TxMessage& m = item.m_message;

    pm.m_capcode = m.m_capcode;
    pm.m_type = m.m_type;
    pm.m_text = m.m_text;
    pm.m_priority = m.m_priority;
    pm.m_number = item.m_number;
    pm.m_retrieval = item.m_retrieval;
    pm.m_shortType = m.m_shortType;
    pm.m_source = m.m_source;
    pm.m_binary = m.m_binary;
    pm.m_instruction = m.m_instruction;
    pm.m_numericFormat = m.m_numericFormat;
    pm.m_fragment = item.m_fragment;
    pm.m_fragments = item.m_fragments;
    pm.m_signature = item.m_signature;
    toneOnly(pm);
    return pm;
}

bool RadioPagingFlexEncoder::refragment(std::list<Item>::iterator item, int messageWords)
{
    const RadioPagingFlex::MessageType type = item->m_message.m_type;

    if ((item->m_group >= 0) || ((type != RadioPagingFlex::Alphanumeric) && (type != RadioPagingFlex::Binary)
        && (type != RadioPagingFlex::Secure))) {
        return false;
    }

    // The rest of the message: this fragment and those after it, still queued in order
    std::vector<std::list<Item>::iterator> rest;
    std::string text;

    for (auto it = item; it != m_queue.end(); ++it)
    {
        if ((it->m_id == item->m_id) && (it->m_fragment >= item->m_fragment))
        {
            rest.push_back(it);
            text += it->m_message.m_text;
        }
    }

    RadioPagingFlex::PhaseMessage pm = phaseMessage(*item);
    pm.m_text = text;
    std::vector<RadioPagingFlex::PhaseMessage> parts;
    std::string error;

    if (!RadioPagingFlex::fragmentMessage(pm, parts, error, item->m_fragment, messageWords) || parts.empty()
        || (RadioPagingFlex::messageCost(parts.front()) < 0)) {
        return false;
    }

    // The pieces replace the fragments: the first one in place of this item, the others after it
    Item model = *item;

    for (size_t k = 1; k < rest.size(); k++) {
        m_queue.erase(rest[k]);
    }

    auto at = std::next(item);

    for (size_t k = 0; k < parts.size(); k++)
    {
        Item piece = model;
        piece.m_message.m_text = parts[k].m_text;
        piece.m_fragment = parts[k].m_fragment;
        piece.m_fragments = parts[k].m_fragments;
        piece.m_signature = (k == 0) ? model.m_signature : -1;

        if (k == 0) {
            *item = piece;
        } else {
            m_queue.insert(at, piece);
        }
    }

    return true;
}

// Tone only is sent as a Short Message / Tone Only vector with the tone only payload: numeric, every digit a space
// (STD-43A Table 3.9.2-1). An address without a vector, which the standard also allows for tone only, is not sent:
// pagers do not take it
void RadioPagingFlexEncoder::toneOnly(RadioPagingFlex::PhaseMessage& pm)
{
    if (pm.m_type == RadioPagingFlex::Tone)
    {
        pm.m_type = RadioPagingFlex::ShortMessage;
        pm.m_shortType = RadioPagingFlex::ShortNumeric;
        pm.m_text.clear();
    }
}

bool RadioPagingFlexEncoder::fragmentReady(const Item& item) const
{
    // The group message goes once the short instructions of all its pagers have gone
    if (item.m_delivery) {
        return groupReady(item);
    }

    if (item.m_fragment > 0)
    {
        // After the fragment before it (that one is erased from the queue once its frame is built)
        for (const Item& other : m_queue)
        {
            if ((other.m_id == item.m_id) && (other.m_fragment < item.m_fragment)) {
                return false;
            }
        }

        return true;
    }
    if (item.m_fragments <= 1) {
        return true;
    }

    // The first fragment of a fragmented message waits until a fragmented message to the same address
    // queued before it has been sent entirely (STD-43A 3.8.5 (2), 4.2 (1))
    for (const Item& other : m_queue)
    {
        if (&other == &item) {
            break;
        }
        if ((other.m_id != item.m_id) && (other.m_fragments > 1) && (other.m_message.m_capcode == item.m_message.m_capcode)) {
            return false;
        }
    }

    return true;
}

bool RadioPagingFlexEncoder::frameFor(const Item& item, uint32_t frame) const
{
    if (m_reserved[frame]) {
        return false;
    }

    // A message with its own frame (the group message of a temporary address)
    if (item.m_message.m_frame >= 0) {
        return (int) frame == item.m_message.m_frame;
    }

    // The first fragment and an unfragmented message go in a frame of the pager (its collapse); the
    // pager then receives the frames that follow until the last fragment, regardless of the collapse
    // (STD-43A 4.2.1 (1)): the other fragments go in any frame
    if (item.m_fragment > 0) {
        return true;
    }

    return RadioPagingFlexScheduler::frameEligible(item.m_message.m_capcode, frame, m_collapse);
}

RadioPagingFlexEncoder::EnqueueResult RadioPagingFlexEncoder::enqueue(const TxMessage& message)
{
    EnqueueResult result;
    RadioPagingFlex::PhaseMessage pm;

    if (!message.m_members.empty()) {
        return enqueueGroup(message);
    }
    if ((message.m_frame < -1) || (message.m_frame >= RadioPagingFlexScheduler::m_framesPerCycle))
    {
        result.m_error = "Frame 0 to 127";
        return result;
    }

    pm.m_capcode = message.m_capcode;
    pm.m_type = message.m_type;
    pm.m_text = (message.m_type == RadioPagingFlex::Tone) ? std::string() : message.m_text;
    pm.m_priority = message.m_priority;
    pm.m_shortType = message.m_shortType;
    pm.m_source = message.m_source;
    pm.m_binary = message.m_binary;
    pm.m_instruction = message.m_instruction;
    pm.m_numericFormat = message.m_numericFormat;
    toneOnly(pm);

    // The numbered short message and the numbered numeric format tell that the network numbers the messages (STD-43A
    // 3.9.2, 3.10.1.1.2, 8.4)
    if ((((pm.m_type == RadioPagingFlex::ShortMessage) && (pm.m_shortType == RadioPagingFlex::ShortNumbered))
        || ((pm.m_type == RadioPagingFlex::Numeric) && RadioPagingFlex::isNumberedFormat(pm.m_numericFormat))) && !m_numbering.m_enabled)
    {
        result.m_error = "Numbered short message and numbered numeric: the network has no message numbering";
        return result;
    }

    if ((message.m_retransmit < 0) || (message.m_retransmit > m_maxRetransmit)
        || (message.m_retransmitInterval < 1) || (message.m_retransmitInterval > m_maxScheduleFrames)
        || (message.m_sendDelay < 0) || (message.m_sendDelay > m_maxScheduleFrames))
    {
        result.m_error = "Retransmissions 0 to 15, retransmission interval 1 to 1920 frames, send delay 0 to 1920 frames";
        return result;
    }

    // A retransmission is a numbered message sent again with its number and R = 0 (STD-43A 4.3.7.6, 8.4): the pager
    // takes it as the message it may have missed
    const bool numbered = (pm.m_type == RadioPagingFlex::Alphanumeric) || (pm.m_type == RadioPagingFlex::Binary)
        || ((pm.m_type == RadioPagingFlex::Numeric) && RadioPagingFlex::isNumberedFormat(pm.m_numericFormat))
        || ((pm.m_type == RadioPagingFlex::ShortMessage) && (pm.m_shortType == RadioPagingFlex::ShortNumbered));

    if ((message.m_retransmit > 0) && (!numbered || !m_numbering.m_enabled))
    {
        result.m_error = "Retransmission: alphanumeric, HEX/binary, numbered numeric and numbered short messages of a network with message numbering";
        return result;
    }

    // Alphanumeric, HEX/binary and secure messages are split into fragments of a frame each; the others are one piece
    std::vector<RadioPagingFlex::PhaseMessage> parts;

    if ((pm.m_type == RadioPagingFlex::Alphanumeric) || (pm.m_type == RadioPagingFlex::Binary) || (pm.m_type == RadioPagingFlex::Secure))
    {
        if (!RadioPagingFlex::fragmentMessage(pm, parts, result.m_error)) {
            return result;
        }
    }
    else
    {
        parts.push_back(pm);
    }

    for (const RadioPagingFlex::PhaseMessage& part : parts)
    {
        int cost = RadioPagingFlex::messageCost(part, &result.m_error);

        if (cost < 0) {
            return result;
        }
        if (cost > RadioPagingFlex::m_wordsPerFrame - 1)
        {
            result.m_error = "Message does not fit in a frame";
            return result;
        }
    }
    if ((message.m_frame >= 0) ? m_reserved[message.m_frame] : !schedulable(message.m_capcode))
    {
        result.m_error = "All frames of the capcode are reserved";
        return result;
    }

    const uint32_t id = m_nextId++;
    std::vector<Item> items;

    for (const RadioPagingFlex::PhaseMessage& part : parts)
    {
        Item item;
        item.m_id = id;
        item.m_message = message;
        item.m_message.m_text = part.m_text;
        item.m_number = -1;
        item.m_fragment = part.m_fragment;
        item.m_fragments = part.m_fragments;
        item.m_signature = part.m_signature;
        item.m_delay = message.m_sendDelay;
        item.m_retransmitsLeft = message.m_retransmit;
        items.push_back(item);
    }

    m_queue.insert(m_queue.end(), items.begin(), items.end());

    if (message.m_retransmit > 0) {
        m_templates[id] = items;
    }

    result.m_ok = true;
    result.m_id = id;
    return result;
}

// A group message (STD-43A 5.2): a short instruction to each pager gives it the temporary address and the frame of
// the group message; the group message goes to the temporary address in that frame, in the phase of the pagers
RadioPagingFlexEncoder::EnqueueResult RadioPagingFlexEncoder::enqueueGroup(const TxMessage& message)
{
    EnqueueResult result;
    const bool tone = (message.m_type == RadioPagingFlex::Tone);
    const bool shortMessage = (message.m_type == RadioPagingFlex::ShortMessage) && (message.m_shortType != RadioPagingFlex::ShortNumbered);

    if ((message.m_members.size() < 2) || ((int) message.m_members.size() > m_maxGroupMembers))
    {
        result.m_error = "A group has 2 to " + std::to_string(m_maxGroupMembers) + " pagers";
        return result;
    }
    if ((message.m_type != RadioPagingFlex::Numeric) && (message.m_type != RadioPagingFlex::Alphanumeric) && !tone && !shortMessage)
    {
        result.m_error = "A group message is numeric, alphanumeric, tone only or a short message that is not numbered";
        return result;
    }
    if (message.m_retransmit != 0)
    {
        result.m_error = "A group message is not sent again";
        return result;
    }
    if ((message.m_type == RadioPagingFlex::Numeric) && RadioPagingFlex::isNumberedFormat(message.m_numericFormat))
    {
        result.m_error = "A group message has no message number";
        return result;
    }
    if (message.m_frame >= 0)
    {
        result.m_error = "A group message goes in the frame of its short instructions";
        return result;
    }
    if (m_collapse > m_maxGroupCollapse)
    {
        result.m_error = "A group message needs a collapse of 6 or less: it follows its short instructions within 128 frames";
        return result;
    }
    if ((message.m_slot < -1) || (message.m_slot >= (int) RadioPagingFlex::m_temporaryCount))
    {
        result.m_error = "Temporary address 0 to 15";
        return result;
    }
    if ((message.m_sendDelay < 0) || (message.m_sendDelay > m_maxScheduleFrames))
    {
        result.m_error = "Send delay 0 to 1920 frames";
        return result;
    }

    for (size_t i = 0; i < message.m_members.size(); i++)
    {
        uint64_t member = message.m_members[i];

        if (!RadioPagingFlex::isValidAddress(member))
        {
            result.m_error = "Capcode out of range";
            return result;
        }
        if (std::find(message.m_members.begin(), message.m_members.begin() + (long) i, member) != message.m_members.begin() + (long) i)
        {
            result.m_error = "A pager is twice in the group";
            return result;
        }
        if (!schedulable(member))
        {
            result.m_error = "All frames of a capcode of the group are reserved";
            return result;
        }
    }

    int slot = message.m_slot;
    const int phases = RadioPagingFlex::phaseCount(m_mode);
    std::map<int, int> phaseSlots;      // Phase of the pagers to temporary address

    for (uint64_t member : message.m_members) {
        phaseSlots[(int) (RadioPagingFlexScheduler::capcodeSlot(member).m_phase % (uint32_t) phases)] = -1;
    }

    // The short instruction of a pager goes in a frame it listens to (one in 2^collapse), before the group frame,
    // within 128 frames of the first instruction (STD-43A 5.2.1): the pagers of a phase that listen to the same frames
    // share those frames
    {
        std::map<std::pair<int, uint32_t>, int> sameFrames;
        const uint32_t period = 1u << m_collapse;
        const int frames = (RadioPagingFlexScheduler::m_framesPerCycle - 1) >> m_collapse;

        for (uint64_t member : message.m_members)
        {
            RadioPagingFlexScheduler::CapcodeSlot s = RadioPagingFlexScheduler::capcodeSlot(member);
            sameFrames[std::make_pair((int) (s.m_phase % (uint32_t) phases), s.m_frame % period)]++;
        }

        for (const auto& entry : sameFrames)
        {
            if ((entry.second + m_instructionsPerFrame - 1) / m_instructionsPerFrame > frames)
            {
                result.m_error = "At most " + std::to_string(frames * m_instructionsPerFrame) + " pagers of a phase that listen to "
                    "the same frames (collapse " + std::to_string(m_collapse) + "): their short instructions go before the group message";
                return result;
            }
        }
    }

    // One temporary address for each phase of the pagers: a group message to one address in more than two phases
    // would use the address more than twice in the frame (STD-43A 3.8.5 (1))
    if ((slot >= 0) && ((phaseSlots.size() > 1) || m_slotBusy[slot]))
    {
        result.m_error = (phaseSlots.size() > 1) ? "A temporary address can be chosen for pagers of one phase" : "The temporary address is in use";
        return result;
    }

    std::vector<bool> taken(RadioPagingFlex::m_temporaryCount, false);

    for (auto& entry : phaseSlots)
    {
        if (slot >= 0)
        {
            entry.second = slot;
            continue;
        }

        for (int s = 0; (entry.second < 0) && (s < (int) RadioPagingFlex::m_temporaryCount); s++)
        {
            if (!m_slotBusy[s] && !taken[s])
            {
                entry.second = s;
                taken[s] = true;
            }
        }
        if (entry.second < 0)
        {
            result.m_error = "No temporary address is free: the group messages in progress use them all";
            return result;
        }
    }

    Group group;
    group.m_slots = phaseSlots;
    group.m_members = message.m_members;
    group.m_message = message;
    group.m_message.m_members.clear();
    group.m_message.m_capcode = RadioPagingFlex::m_temporaryBase + (uint64_t) phaseSlots.begin()->second;
    group.m_message.m_sendDelay = 0;

    // The group message and one short instruction have to fit a frame
    RadioPagingFlex::PhaseMessage pm = phaseMessage(Item{0, group.m_message, 0});
    std::string error;

    if (RadioPagingFlex::messageCost(pm, &error) < 0)
    {
        result.m_error = error;
        return result;
    }

    TxMessage instruction;
    instruction.m_type = RadioPagingFlex::ShortInstruction;
    instruction.m_instruction.m_type = RadioPagingFlex::InstructionParams::TemporaryAddress;
    instruction.m_sendDelay = message.m_sendDelay;

    const int key = m_nextGroup++;
    const uint32_t id = m_nextId++;
    group.m_id = id;
    std::vector<Item> items;

    for (uint64_t member : message.m_members)
    {
        Item item;
        item.m_id = id;
        item.m_message = instruction;
        item.m_message.m_capcode = member;
        item.m_message.m_instruction.m_slot = phaseSlots[(int) (RadioPagingFlexScheduler::capcodeSlot(member).m_phase % (uint32_t) phases)];
        item.m_number = 0;
        item.m_delay = message.m_sendDelay;
        item.m_group = key;
        items.push_back(item);
    }

    m_queue.insert(m_queue.end(), items.begin(), items.end());
    m_groups[key] = group;

    for (const auto& entry : phaseSlots) {
        m_slotBusy[entry.second] = true;
    }
    result.m_ok = true;
    result.m_id = id;
    return result;
}

// The first short instruction of a group that may go fixes the frame of the group message: after the frames the
// pagers listen to, one more for the instructions that wait for room, and not the frame right after the last
// instruction (STD-43A 5.2.1)
void RadioPagingFlexEncoder::setGroupTargets(uint64_t frameIndex)
{
    for (auto& entry : m_groups)
    {
        Group& group = entry.second;

        if (group.m_targetSet) {
            continue;
        }

        bool started = false;

        for (const Item& item : m_queue)
        {
            if ((item.m_group == entry.first) && !item.m_delivery && due(item, frameIndex)) {
                started = true;
            }
        }
        if (!started) {
            continue;
        }

        // Frames the short instructions take: the busiest phase at most m_instructionsPerFrame a frame, a pager in its
        // frames only (one in 2^collapse); the group message within 128 frames of the first instruction
        const int phases = RadioPagingFlex::phaseCount(m_mode);
        std::map<int, int> perPhase;

        for (uint64_t member : group.m_members) {
            perPhase[(int) (RadioPagingFlexScheduler::capcodeSlot(member).m_phase % (uint32_t) phases)]++;
        }

        int busiest = 0;

        for (const auto& p : perPhase) {
            busiest = std::max(busiest, p.second);
        }

        const uint64_t instructionFrames = (uint64_t) ((busiest + m_instructionsPerFrame - 1) / m_instructionsPerFrame) << m_collapse;
        uint64_t target = frameIndex + std::min<uint64_t>((1u << m_collapse) + 2 + instructionFrames, RadioPagingFlexScheduler::m_framesPerCycle - 1);

        for (int tries = 0; (tries < RadioPagingFlexScheduler::m_framesPerCycle) && m_reserved[target % RadioPagingFlexScheduler::m_framesPerCycle]; tries++) {
            target++;
        }

        const int frame = (int) (target % RadioPagingFlexScheduler::m_framesPerCycle);

        for (Item& item : m_queue)
        {
            if ((item.m_group == entry.first) && !item.m_delivery) {
                item.m_message.m_instruction.m_frame = frame;
            }
        }

        group.m_targetSet = true;
        group.m_targetIndex = target;

        // A group message in the phase of each pager, to the temporary address of the phase
        for (const auto& phaseSlot : group.m_slots)
        {
            Item item;
            item.m_id = group.m_id;
            item.m_message = group.m_message;
            item.m_message.m_capcode = RadioPagingFlex::m_temporaryBase + (uint64_t) phaseSlot.second;
            item.m_message.m_frame = frame;
            item.m_number = -1;
            item.m_phase = phaseSlot.first;
            item.m_delay = 0;
            item.m_notBefore = (int64_t) target;
            item.m_group = entry.first;
            item.m_delivery = true;
            m_queue.push_back(item);
        }
    }
}

bool RadioPagingFlexEncoder::groupReady(const Item& item) const
{
    for (const Item& other : m_queue)
    {
        if ((other.m_group == item.m_group) && !other.m_delivery) {
            return false;
        }
    }

    return true;
}

void RadioPagingFlexEncoder::endGroup(int key)
{
    auto it = m_groups.find(key);

    if (it != m_groups.end())
    {
        for (const auto& phaseSlot : it->second.m_slots) {
            m_slotBusy[phaseSlot.second] = false;
        }

        m_groups.erase(it);
    }
}

// A group message whose frame came and went without all its short instructions sent, or without its own message sent
// (the pagers listen to it in that frame only): dropped
void RadioPagingFlexEncoder::failLateGroups(uint64_t frameIndex, Frame& frame)
{
    std::vector<int> late;

    for (const auto& entry : m_groups)
    {
        if (!entry.second.m_targetSet) {
            continue;
        }

        bool instructions = false;
        bool delivery = false;

        for (const Item& item : m_queue)
        {
            if (item.m_group == entry.first) {
                (item.m_delivery ? delivery : instructions) = true;
            }
        }

        if (((frameIndex >= entry.second.m_targetIndex) && instructions) || ((frameIndex > entry.second.m_targetIndex) && delivery)) {
            late.push_back(entry.first);
        }
    }

    for (int key : late)
    {
        frame.m_failedIds.push_back(m_groups[key].m_id);

        for (auto it = m_queue.begin(); it != m_queue.end();)
        {
            if (it->m_group == key) {
                it = m_queue.erase(it);
            } else {
                ++it;
            }
        }

        endGroup(key);
    }
}

bool RadioPagingFlexEncoder::due(const Item& item, uint64_t frameIndex)
{
    if (item.m_notBefore < 0) {
        return item.m_delay == 0;   // Not seen by a frame yet: the delay starts with the first frame built
    }

    return (int64_t) frameIndex >= item.m_notBefore;
}

std::vector<uint32_t> RadioPagingFlexEncoder::unschedulable() const
{
    std::vector<uint32_t> ids;

    for (const Item& item : m_queue)
    {
        // Once per message (its first queued fragment); a group message has its own frame
        if (!item.m_delivery && (item.m_message.m_frame < 0) && !schedulable(item.m_message.m_capcode)
            && (std::find(ids.begin(), ids.end(), item.m_id) == ids.end())) {
            ids.push_back(item.m_id);
        }
    }

    return ids;
}

bool RadioPagingFlexEncoder::remove(uint32_t id)
{
    // All the fragments still queued of the message
    bool found = false;

    for (auto it = m_queue.begin(); it != m_queue.end();)
    {
        if (it->m_id == id)
        {
            it = m_queue.erase(it);
            found = true;
        }
        else
        {
            ++it;
        }
    }

    m_templates.erase(id);

    for (auto it = m_groups.begin(); it != m_groups.end();)
    {
        if (it->second.m_id == id)
        {
            for (const auto& phaseSlot : it->second.m_slots) {
                m_slotBusy[phaseSlot.second] = false;
            }

            it = m_groups.erase(it);
            found = true;
        }
        else
        {
            ++it;
        }
    }

    return found;
}

size_t RadioPagingFlexEncoder::eligible(uint64_t frameIndex) const
{
    uint32_t frame = RadioPagingFlexScheduler::frameTime(frameIndex).m_frame;
    size_t n = 0;

    if (m_reserved[frame]) {
        return 0;
    }

    for (const Item& item : m_queue)
    {
        if (frameFor(item, frame) && fragmentReady(item) && due(item, frameIndex)) {
            n++;
        }
    }

    return n;
}

bool RadioPagingFlexEncoder::mandatory(uint64_t frameIndex) const
{
    RadioPagingFlexScheduler::FrameTime time = RadioPagingFlexScheduler::frameTime(frameIndex);

    if (m_reserved[time.m_frame]) {
        return false;
    }

    // Frame 0 of cycle 0 carries the time in every phase (STD-43A 3.7.2)
    if (m_time.m_enabled && (time.m_cycle == 0) && (time.m_frame == 0)) {
        return true;
    }
    // The System Event Notification goes in each frame (STD-43A 3.8.2.4)
    if (m_eventFrames > 0) {
        return true;
    }

    for (const Item& item : m_queue)
    {
        if ((item.m_fragment > 0) && fragmentReady(item) && due(item, frameIndex)) {
            return true;
        }
        // The group message goes in its frame only
        if (item.m_delivery && frameFor(item, time.m_frame) && fragmentReady(item) && due(item, frameIndex)) {
            return true;
        }
    }

    return false;
}

bool RadioPagingFlexEncoder::notifySystemEvent(unsigned events, int frames, std::string& error)
{
    if ((events == 0) || (events > m_maxEvents))
    {
        error = "System event flags must be 1 to 2047 (e10-e0)";
        return false;
    }
    if ((frames < m_minEventFrames) || (frames > m_maxEventFrames))
    {
        error = "The System Event Notification goes in each frame for 128 to 512 frames (one to 4 cycles)";
        return false;
    }

    m_events = events;
    m_eventFrames = frames;
    return true;
}

bool RadioPagingFlexEncoder::setTimeService(const TimeService& time, std::string& error)
{
    if (time.m_sendZone && (RadioPagingFlex::timeZoneCode(time.m_zoneMinutes) < 0))
    {
        error = "Time zone offset " + std::to_string(time.m_zoneMinutes) + " minutes is not in STD-43A Table 3.7.2-3";
        return false;
    }
    if (time.m_extendedSeconds > 7)
    {
        error = "Extended seconds must be 0 to 7 (steps of 0.9375 s)";
        return false;
    }

    m_time = time;
    return true;
}

int64_t RadioPagingFlexEncoder::localTimeMs(uint64_t frameIndex) const
{
    RadioPagingFlexScheduler::FrameTime time = RadioPagingFlexScheduler::frameTime(frameIndex);
    // The time of frame 0 of the cycle: the synchronization is on the first bit of bit sync 1 of that frame
    uint64_t reference = (m_time.m_reference == TimeService::FrameZero) ? frameIndex - time.m_frame : frameIndex;

    return (int64_t) reference * RadioPagingFlex::m_frameMs + m_time.m_offsetSeconds * 1000
        + ((int64_t) m_time.m_zoneMinutes + (m_time.m_daylightSaving ? 60 : 0)) * 60000;
}

std::vector<uint32_t> RadioPagingFlexEncoder::extraBlockInfo(uint64_t frameIndex, int phase) const
{
    // Block information words 2 to 4: at most 3 (the address start field a1 a0 counts 1 to 4 block words)
    const size_t maxExtra = 3;
    RadioPagingFlexScheduler::FrameTime time = RadioPagingFlexScheduler::frameTime(frameIndex);
    std::vector<uint32_t> words;

    // The simulcast system ID words are in one phase of the frame (STD-43A 6.1.1.3): word 1 in every frame,
    // word 2 in frames 0 to 3
    if (m_roaming.m_enabled && (phase == RadioPagingFlex::ssidPhase(m_mode, time.m_frame)))
    {
        words.push_back(RadioPagingFlex::ssid1Word(m_roaming.m_localId, m_roaming.m_coverageZone));

        if (time.m_frame < (uint32_t) RadioPagingFlexScheduler::m_protectedFrames) {
            words.push_back(RadioPagingFlex::ssid2Word(m_roaming.m_countryCode, m_roaming.m_trafficFlags));
        }
    }

    if (m_time.m_enabled && (words.size() < maxExtra))
    {
        int64_t ms = localTimeMs(frameIndex);
        int64_t seconds = (ms >= 0) ? ms / 1000 : -((-ms + 999) / 1000);
        int year;
        unsigned month, day, hour, minute, second;
        RadioPagingFlex::civilTime(seconds, year, month, day, hour, minute, second);
        // The second in 7.5 s steps (eighths of minute) in the time word; the extended second of the zone word is
        // the correction set
        unsigned steps = second * 8 / 60;

        if (m_time.m_equivalentYear) {
            year = RadioPagingFlex::equivalentYear(year);
        }

        // In the order date, time, time zone; the frame index modulo 3 chooses the first when they do not all fit
        std::vector<uint32_t> timeWords;
        timeWords.push_back(RadioPagingFlex::dateWord(year, month, day));
        timeWords.push_back(RadioPagingFlex::timeWord(hour, minute, steps));
        if (m_time.m_sendZone) {
            timeWords.push_back(RadioPagingFlex::timeZoneWord((unsigned) RadioPagingFlex::timeZoneCode(m_time.m_zoneMinutes),
                m_time.m_daylightSaving, m_time.m_extendedSeconds));
        }

        size_t room = maxExtra - words.size();
        size_t first = (room < timeWords.size()) ? (size_t) (frameIndex % 3) % timeWords.size() : 0;

        for (size_t i = 0; (i < timeWords.size()) && (i < room); i++) {
            words.push_back(timeWords[(first + i) % timeWords.size()]);
        }
    }

    return words;
}

RadioPagingFlexEncoder::Frame RadioPagingFlexEncoder::buildFrame(uint64_t frameIndex)
{
    Frame frame;
    RadioPagingFlexScheduler::FrameTime time = RadioPagingFlexScheduler::frameTime(frameIndex);

    frame.m_cycle = time.m_cycle;
    frame.m_frame = time.m_frame;
    frame.m_mode = m_mode;
    frame.m_inverted = m_inverted;

    // The send delay of the messages queued since the last frame starts with this one
    for (Item& item : m_queue)
    {
        if (item.m_notBefore < 0) {
            item.m_notBefore = (int64_t) frameIndex + item.m_delay;
        }
    }

    failLateGroups(frameIndex, frame);
    setGroupTargets(frameIndex);
    if (m_reserved[time.m_frame])
    {
        frame.m_reserved = true;
        return frame;
    }

    const int phases = RadioPagingFlex::phaseCount(m_mode);
    const int capacity = RadioPagingFlex::m_wordsPerFrame - 1;
    const bool twoLevels = (RadioPagingFlex::levels(m_mode) == 2);

    struct Candidate
    {
        std::list<Item>::iterator m_item;
        int m_cost;
        int m_addressWords;
    };

    std::vector<Candidate> candidates[RadioPagingFlex::m_maxPhases];
    // Block information words 2 to 4 of each phase (system ID of roaming, time): they take words of the phase
    std::vector<uint32_t> extraBiw[RadioPagingFlex::m_maxPhases];
    int used[RadioPagingFlex::m_maxPhases] = {0, 0, 0, 0};
    for (int p = 0; p < phases; p++)
    {
        extraBiw[p] = extraBlockInfo(frameIndex, p);
        used[p] = (int) extraBiw[p].size();
    }

    // The System Event Notification in every phase, ahead of the messages (STD-43A 3.8.2.4)
    RadioPagingFlex::PhaseMessage event;
    const bool notify = (m_eventFrames > 0);

    if (notify)
    {
        event.m_capcode = RadioPagingFlex::m_systemEventAddress;
        event.m_type = RadioPagingFlex::ShortInstruction;
        event.m_instruction.m_type = RadioPagingFlex::InstructionParams::SystemEvent;
        event.m_instruction.m_events = m_events;
        int eventCost = std::max(0, RadioPagingFlex::messageCost(event));

        for (int p = 0; p < phases; p++) {
            used[p] += eventCost;
        }

        m_eventFrames--;
        frame.m_systemEvent = true;
    }

    // The words of each phase before the messages: its block information words 2 to 4 and the System Event
    // Notification. A message that does not fit what is left is cut again into fragments (fitByFragments)
    int reserved[RadioPagingFlex::m_maxPhases];
    std::copy(used, used + RadioPagingFlex::m_maxPhases, reserved);
    int priorityWords[RadioPagingFlex::m_maxPhases] = {0, 0, 0, 0};

    // Use of each address in the frame (STD-43A 3.8.5): unfragmented messages twice at most, once only
    // while the address is sending a fragmented message (from its first to its last fragment)
    struct Usage
    {
        uint64_t m_capcode;
        int m_unfragmented;
        bool m_fragmenting;
    };
    std::vector<Usage> usage;
    auto usageOf = [&](uint64_t capcode) -> Usage& {
        for (Usage& u : usage)
        {
            if (u.m_capcode == capcode) {
                return u;
            }
        }
        usage.push_back(Usage{capcode, 0, false});
        return usage.back();
    };

    // Addresses with a fragmented message under way: its first fragment has gone
    for (const Item& item : m_queue)
    {
        if (item.m_fragment > 0) {
            usageOf(item.m_message.m_capcode).m_fragmenting = true;
        }
    }

    // Phases of the most significant bit of 4-level symbols are preferred when a phase is full
    auto isMsb = [&](int p) {
        if (twoLevels) {
            return true;
        }
        return (phases == 2) ? (p == 0) : (phases == 4) ? ((p == 0) || (p == 2)) : true;
    };

    // Passes: fragments after the first (the pager is waiting for them: they keep the interval of
    // STD-43A 4.2 (4)), priority addresses, other addresses, tone only addresses. Messages keep their
    // queue order. The address field puts the priority addresses first whatever the pass
    for (int pass = 0; pass < 4; pass++)
    {
        for (auto it = m_queue.begin(); it != m_queue.end(); ++it)
        {
            const TxMessage& m = it->m_message;
            bool isTone = (m.m_type == RadioPagingFlex::Tone);
            bool continuation = (it->m_fragment > 0) || it->m_delivery;

            if (!frameFor(*it, time.m_frame) || !fragmentReady(*it) || !due(*it, frameIndex)) {
                continue;
            }
            if ((pass == 0) && !continuation) {
                continue;
            }
            if ((pass == 1) && (continuation || !m.m_priority || isTone)) {
                continue;
            }
            if ((pass == 2) && (continuation || m.m_priority || isTone)) {
                continue;
            }
            if ((pass == 3) && !isTone) {
                continue;
            }

            int cost = RadioPagingFlex::messageCost(phaseMessage(*it));
            if (cost < 0) {
                continue;
            }

            Usage& use = usageOf(m.m_capcode);

            if (it->m_fragments > 1)
            {
                // One fragmented message at a time per address (fragmentReady), and a first fragment
                // only when the address has at most one unfragmented message in the frame
                if ((it->m_fragment == 0) && (use.m_unfragmented > 1)) {
                    continue;
                }
            }
            else if (use.m_unfragmented >= (use.m_fragmenting ? 1 : 2))
            {
                continue;
            }

            int addressWords = RadioPagingFlex::isLongAddress(m.m_capcode) ? 2 : 1;
            int phase = (int) (RadioPagingFlexScheduler::capcodeSlot(m.m_capcode).m_phase % (uint32_t) phases);

            // The fragments of a message stay in the phase of its first fragment: the pager reads that phase
            if (it->m_phase >= 0) {
                phase = it->m_phase % phases;
            }

            // A message of fragments (alphanumeric, HEX/binary, secure) is cut again to the room of its phase in this
            // frame (STD-43A 4.1: a fragment is what the frame has room for, after its block information words and the
            // other messages), the rest in the next frames: a fragmented message (its pager waits for the next
            // fragment, 4.2 (4)), or one that does not fit even a frame of no other message. Another message waits for
            // a frame with room for it whole
            auto fitByFragments = [&](int p) {
                // Address and vector words: a long address has 2 of each, the second vector word carrying the first
                // message word (STD-43A 3.9), so 3 words besides the message words
                const int overhead = RadioPagingFlex::isLongAddress(m.m_capcode) ? 3 : 2;
                const int room = capacity - used[p] - overhead;
                const bool fragmented = (it->m_fragments > 1) || (it->m_fragment > 0);

                if (!fragmented && (reserved[p] + cost <= capacity)) {
                    return false;
                }
                if ((room < 2) || !refragment(it, room)) {
                    return false;
                }

                cost = RadioPagingFlex::messageCost(phaseMessage(*it));
                return (cost >= 0) && (used[p] + cost <= capacity);
            };

            if (used[phase] + cost > capacity)
            {
                // The fragments of a message stay in the phase of the first one
                if ((phases <= 1) || (it->m_phase >= 0))
                {
                    if (!fitByFragments(phase)) {
                        continue;
                    }
                }
                else
                {
                    int found = -1;
                    for (int p = 0; (p < phases) && (found < 0); p++)
                    {
                        if ((p != phase) && isMsb(p) && (used[p] + cost <= capacity)) {
                            found = p;
                        }
                    }
                    for (int p = 0; (p < phases) && (found < 0); p++)
                    {
                        if ((p != phase) && !isMsb(p) && (used[p] + cost <= capacity)) {
                            found = p;
                        }
                    }
                    if (found >= 0) {
                        phase = found;
                    } else if (!fitByFragments(phase)) {
                        continue;
                    }
                }
            }

            if (m.m_priority && (priorityWords[phase] + addressWords > RadioPagingFlex::m_maxPriorityWords))
            {
                if ((phases <= 1) || (it->m_phase >= 0)) {
                    continue;
                }

                int found = -1;
                for (int p = 0; p < phases; p++)
                {
                    if ((p == phase) || (priorityWords[p] + addressWords > RadioPagingFlex::m_maxPriorityWords)
                        || (used[p] + cost > capacity)) {
                        continue;
                    }
                    if ((found < 0) || isMsb(p))
                    {
                        found = p;
                        if (isMsb(p)) {
                            break;
                        }
                    }
                }
                if (found < 0) {
                    continue;
                }
                phase = found;
            }

            if ((int) candidates[phase].size() >= maxCandidates) {
                continue;
            }

            Candidate c;
            c.m_item = it;
            c.m_cost = cost;
            c.m_addressWords = addressWords;
            candidates[phase].push_back(c);
            used[phase] += cost;
            if (m.m_priority) {
                priorityWords[phase] += addressWords;
            }

            if (it->m_fragments > 1) {
                use.m_fragmenting = true;
            } else {
                use.m_unfragmented++;
            }
        }
    }

    // Low traffic flags from the address field length of each phase
    std::vector<int> addressWords(phases, 0);
    for (int p = 0; p < phases; p++)
    {
        for (const Candidate& c : candidates[p]) {
            addressWords[p] += c.m_addressWords;
        }
        // The flag tells that the address field ends in block 0 (STD-43A 3.6): block information words 2 to 4
        // come first
        addressWords[p] += (int) extraBiw[p].size() + (notify ? 1 : 0);
    }
    frame.m_traffic = RadioPagingFlex::lowTrafficFlags(addressWords, phases);

    // Phases
    std::vector<std::vector<uint32_t>> phaseWords(phases);
    struct Sent
    {
        std::list<Item>::iterator m_item;
        int m_phase;                    //!< The phase it went in
    };
    std::vector<Sent> sent;

    for (int p = 0; p < phases; p++)
    {
        RadioPagingFlex::PhaseParams params;
        params.m_mode = m_mode;
        params.m_phase = p;
        params.m_collapse = m_collapse;
        params.m_carryOn = 0;

        params.m_extraBiw = extraBiw[p];

        if (!candidates[p].empty() || notify)
        {
            std::vector<RadioPagingFlex::PhaseMessage> messages;
            const size_t first = notify ? 1 : 0;   // The notification goes first

            if (notify) {
                messages.push_back(event);
            }

            for (const Candidate& c : candidates[p])
            {
                Item& item = *c.m_item;

                if (item.m_number < 0) {
                    assignNumber(item);
                }

                messages.push_back(phaseMessage(item));
            }

            RadioPagingFlex::PhaseResult result = RadioPagingFlex::buildPhase(params, messages);

            if (result.m_ok)
            {
                phaseWords[p] = result.m_words;
                for (size_t j = first; j < messages.size(); j++)
                {
                    if (result.m_packed[j]) {
                        sent.push_back(Sent{candidates[p][j - first].m_item, p});
                    }
                }
            }
        }

        if (phaseWords[p].empty()) {
            phaseWords[p] = RadioPagingFlex::buildPhase(params, std::vector<RadioPagingFlex::PhaseMessage>()).m_words;
        }
    }

    for (const Sent& s : sent)
    {
        auto it = s.m_item;

        // The short instructions of a group go once; the group message is sent with its last phase
        if (it->m_group >= 0)
        {
            const int key = it->m_group;
            const bool delivery = it->m_delivery;
            m_queue.erase(it);

            if (delivery)
            {
                bool more = false;

                for (const Item& other : m_queue) {
                    more = more || ((other.m_group == key) && other.m_delivery);
                }
                if (!more)
                {
                    const uint32_t id = m_groups[key].m_id;
                    // Where the group message went: no message number (temporary address)
                    SentStatus status;
                    status.m_cycle = (int) frame.m_cycle;
                    status.m_frame = (int) frame.m_frame;
                    status.m_phase = s.m_phase;
                    frame.m_sentIds.push_back(id);
                    frame.m_sentNumbers.push_back(0);
                    frame.m_sentStatus.push_back(status);
                    frame.m_doneIds.push_back(id);
                    endGroup(key);
                }
            }

            continue;
        }

        // The fragments still to go take the phase of this one
        if (it->m_fragments > 1)
        {
            for (Item& other : m_queue)
            {
                if ((other.m_id == it->m_id) && (other.m_fragment > it->m_fragment))
                {
                    other.m_phase = s.m_phase;

                    if (it->m_fragment == 0)
                    {
                        other.m_firstCycle = (int) frame.m_cycle;
                        other.m_firstFrame = (int) frame.m_frame;
                    }
                }
            }
        }

        // A message is sent with its last fragment. A message to retransmit is queued again, all its fragments,
        // with its number and R = 0, the interval after this frame (STD-43A 4.3.7.6)
        if (it->m_fragment == it->m_fragments - 1)
        {
            SentStatus status;
            status.m_number = it->m_number;
            status.m_fragments = it->m_fragments;
            status.m_retrieval = it->m_retrieval;
            status.m_cycle = (int) frame.m_cycle;
            status.m_frame = (int) frame.m_frame;
            status.m_phase = s.m_phase;

            if (it->m_fragments > 1)
            {
                status.m_firstCycle = it->m_firstCycle;
                status.m_firstFrame = it->m_firstFrame;
            }

            if (it->m_retransmission)
            {
                frame.m_retransmittedIds.push_back(it->m_id);
                frame.m_retransmittedStatus.push_back(status);
            }
            else
            {
                frame.m_sentIds.push_back(it->m_id);
                frame.m_sentNumbers.push_back((uint32_t) it->m_number);
                frame.m_sentStatus.push_back(status);
            }

            auto tpl = m_templates.find(it->m_id);

            if ((it->m_retransmitsLeft > 0) && (tpl != m_templates.end()))
            {
                for (const Item& part : tpl->second)
                {
                    Item copy = part;
                    copy.m_number = it->m_number;
                    copy.m_retrieval = false;
                    copy.m_phase = -1;
                    copy.m_delay = 0;
                    copy.m_notBefore = (int64_t) frameIndex + it->m_message.m_retransmitInterval;
                    copy.m_retransmitsLeft = it->m_retransmitsLeft - 1;
                    copy.m_retransmission = true;
                    m_queue.push_back(copy);
                }
            }
            else
            {
                m_templates.erase(it->m_id);
                frame.m_doneIds.push_back(it->m_id);
            }
        }
        m_queue.erase(it);
    }
    frame.m_idle = sent.empty() && !notify;

    RadioPagingFlex::FrameParams fp;
    fp.m_mode = m_mode;
    fp.m_cycle = time.m_cycle;
    fp.m_frame = time.m_frame;
    fp.m_traffic = frame.m_traffic;
    fp.m_roaming = m_roaming.m_enabled;
    frame.m_bytes = RadioPagingFlex::assemble(fp, phaseWords);
    frame.m_blocks = RadioPagingFlex::toSymbols(frame.m_bytes, m_mode, m_inverted);
    return frame;
}
