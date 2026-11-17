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

#include <algorithm>

#include "radiopagingtxstream.h"

const int RadioPagingPocsagTxStream::m_idleBatchesBeforeSwitch;
const int RadioPagingFlexTxStream::m_shaping;

// ---------------------------------------------------------------------------
// POCSAG
// ---------------------------------------------------------------------------

namespace {

// Carousel order of the speed and polarity groups
const std::pair<int, bool> carousel[] = {
    {1200, false}, {512, false}, {2400, false},
    {1200, true}, {512, true}, {2400, true}
};
const int carouselSize = sizeof(carousel) / sizeof(carousel[0]);

void appendWord(std::vector<uint8_t>& bits, uint32_t word)
{
    for (int i = 31; i >= 0; i--) {
        bits.push_back((word >> i) & 1);
    }
}

} // namespace

RadioPagingPocsagTxStream::RadioPagingPocsagTxStream(bool network, int baud, bool inverted) :
    m_network(network),
    m_defaultGroup(baud, inverted),
    m_group(baud, inverted),
    m_preambleDue(true),
    m_idleBatches(0),
    m_turnBlocks(0),
    m_inTransmission(false)
{
}

std::vector<float> RadioPagingPocsagTxStream::levels(const std::vector<uint8_t>& bits, bool inverted)
{
    std::vector<float> out;
    out.reserve(bits.size());

    for (uint8_t bit : bits) {
        out.push_back(((bit != 0) != inverted) ? -1.0f : 1.0f);
    }

    return out;
}

std::vector<uint8_t> RadioPagingPocsagTxStream::preambleBits()
{
    std::vector<uint8_t> bits;

    for (int i = 0; i < RadioPagingPocsag::m_preambleBits; i++) {
        bits.push_back((i & 1) ? 0 : 1);
    }

    return bits;
}

std::vector<uint8_t> RadioPagingPocsagTxStream::idleBatchBits()
{
    std::vector<uint8_t> bits;

    appendWord(bits, RadioPagingPocsag::m_syncWord);
    for (int i = 0; i < RadioPagingPocsag::m_codewordsPerBatch; i++) {
        appendWord(bits, RadioPagingPocsag::m_idleWord);
    }

    return bits;
}

bool RadioPagingPocsagTxStream::add(const RadioPagingPocsag::TxMessage& message, int baud, bool inverted,
    uint64_t id, std::string& error)
{
    return add(message, baud, inverted, id, error, Schedule());
}

bool RadioPagingPocsagTxStream::add(const RadioPagingPocsag::TxMessage& message, int baud, bool inverted,
    uint64_t id, std::string& error, const Schedule& schedule)
{
    if ((schedule.m_repeat < 0) || (schedule.m_repeat > m_maxRepeat) || (schedule.m_delayMs < 0) || (schedule.m_delayMs > m_maxScheduleMs)
        || (schedule.m_intervalMs < 0) || (schedule.m_intervalMs > m_maxScheduleMs))
    {
        error = "Repeats 0 to 10, delay and interval 0 to 3600 s";
        return false;
    }
    // A one-shot transmission ends with its last message: nothing waits for later
    if (!m_network && ((schedule.m_repeat > 0) || (schedule.m_delayMs > 0)))
    {
        error = "Repeat and send delay need the network mode";
        return false;
    }

    RadioPagingPocsag::EncodeResult trial = RadioPagingPocsag::encode({ message });

    if (!trial.m_ok)
    {
        error = trial.m_error;
        return false;
    }

    Item item{message, id};
    item.m_delayMs = schedule.m_delayMs;
    item.m_repeatsLeft = schedule.m_repeat;
    item.m_intervalMs = schedule.m_intervalMs;
    Group group(baud, inverted);

    if (m_network) {
        m_queues[group].push_back(item);
    } else {
        m_oneShot.push_back(std::make_pair(group, item));
    }

    return true;
}

// The group being sent is kept: the default takes over when the network starts or has
// nothing to send
void RadioPagingPocsagTxStream::setDefaultGroup(int baud, bool inverted)
{
    m_defaultGroup = Group(baud, inverted);

    if (!m_inTransmission) {
        m_group = m_defaultGroup;
    }
}

bool RadioPagingPocsagTxStream::hasWork() const
{
    return m_network || !m_oneShot.empty();
}

void RadioPagingPocsagTxStream::begin(int64_t epochMs)
{
    m_nowMs = epochMs;
    m_group = m_defaultGroup;
    m_preambleDue = true;
    m_idleBatches = 0;
    m_turnBlocks = 0;
    m_inTransmission = false;
}

size_t RadioPagingPocsagTxStream::pending() const
{
    size_t n = m_oneShot.size();

    for (const auto& q : m_queues) {
        n += q.second.size();
    }

    return n;
}

std::vector<uint64_t> RadioPagingPocsagTxStream::clear()
{
    std::vector<uint64_t> ids;

    for (const auto& entry : m_oneShot) {
        ids.push_back(entry.second.m_id);
    }
    for (const auto& q : m_queues)
    {
        for (const Item& item : q.second)
        {
            // A repeat: the message was reported with its first transmission
            if (!item.m_repeat) {
                ids.push_back(item.m_id);
            }
        }
    }

    m_oneShot.clear();
    m_queues.clear();
    return ids;
}

std::vector<size_t> RadioPagingPocsagTxStream::dueItems(std::deque<Item>& queue)
{
    std::vector<size_t> due;

    for (size_t i = 0; i < queue.size(); i++)
    {
        Item& item = queue[i];

        if (item.m_notBefore < 0) {
            item.m_notBefore = m_nowMs + item.m_delayMs;
        }
        if (m_nowMs >= item.m_notBefore) {
            due.push_back(i);
        }
    }

    return due;
}

void RadioPagingPocsagTxStream::startDelays()
{
    for (auto& q : m_queues)
    {
        for (Item& item : q.second)
        {
            if (item.m_notBefore < 0) {
                item.m_notBefore = m_nowMs + item.m_delayMs;
            }
        }
    }
}

bool RadioPagingPocsagTxStream::hasDue(const std::deque<Item>& queue) const
{
    for (const Item& item : queue)
    {
        if ((item.m_notBefore >= 0) && (m_nowMs >= item.m_notBefore)) {
            return true;
        }
    }

    return false;
}

// Where a POCSAG message went, from its placement in the encoding
static RadioPagingFlexEncoder::SentStatus pocsagStatus(const RadioPagingPocsag::EncodeResult& result, size_t index, int firstBatch)
{
    RadioPagingFlexEncoder::SentStatus status;

    if (index < result.m_placements.size())
    {
        const RadioPagingPocsag::EncodeResult::Placement& placement = result.m_placements[index];
        status.m_batch = firstBatch + placement.m_batch;
        status.m_lastBatch = firstBatch + placement.m_lastBatch;
        status.m_frame = placement.m_frame;
        status.m_codewords = placement.m_codewords;
    }

    return status;
}

void RadioPagingPocsagTxStream::takeSent(std::deque<Item>& queue, const std::vector<size_t>& sent,
    const RadioPagingPocsag::EncodeResult& result, int firstBatch, RadioPagingTxBlock& block)
{
    std::vector<Item> repeats;

    block.m_ids.clear();
    block.m_repeatIds.clear();
    block.m_status.clear();
    block.m_repeatStatus.clear();

    for (size_t k = sent.size(); k-- > 0;)
    {
        Item item = queue[sent[k]];
        queue.erase(queue.begin() + (long) sent[k]);
        const RadioPagingFlexEncoder::SentStatus status = pocsagStatus(result, k, firstBatch);

        if (!item.m_repeat)
        {
            block.m_ids.insert(block.m_ids.begin(), item.m_id);
            block.m_status.insert(block.m_status.begin(), status);
        }
        else
        {
            block.m_repeatIds.insert(block.m_repeatIds.begin(), item.m_id);
            block.m_repeatStatus.insert(block.m_repeatStatus.begin(), status);
        }
        if (item.m_repeatsLeft > 0)
        {
            item.m_repeatsLeft--;
            item.m_repeat = true;
            item.m_notBefore = m_nowMs + item.m_intervalMs;
            repeats.insert(repeats.begin(), item);
        }
    }

    queue.insert(queue.end(), repeats.begin(), repeats.end());
}

// Next group after the current one in the carousel order that has messages
bool RadioPagingPocsagTxStream::nextGroup(Group& group) const
{
    int current = 0;

    for (int i = 0; i < carouselSize; i++)
    {
        if (carousel[i] == m_group) {
            current = i;
        }
    }

    for (int i = 1; i <= carouselSize; i++)
    {
        Group candidate = carousel[(current + i) % carouselSize];
        auto it = m_queues.find(candidate);

        if ((candidate != m_group) && (it != m_queues.end()) && hasDue(it->second))
        {
            group = candidate;
            return true;
        }
    }

    return false;
}

bool RadioPagingPocsagTxStream::next(RadioPagingTxBlock& block)
{
    block = RadioPagingTxBlock();

    if (!m_network)
    {
        // One message, one transmission
        if (m_inTransmission || m_oneShot.empty())
        {
            m_inTransmission = false;
            return false;
        }

        std::pair<Group, Item> entry = m_oneShot.front();
        m_oneShot.pop_front();
        RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode({ entry.second.m_message });

        block.m_baud = entry.first.first;
        block.m_shaping = entry.first.first;
        block.m_levels = levels(result.m_bits, entry.first.second);
        block.m_ids.push_back(entry.second.m_id);
        block.m_status.push_back(pocsagStatus(result, 0, 0));
        m_inTransmission = true;
        return true;
    }

    m_inTransmission = true;
    startDelays();

    // A network always has a block to send: a preamble, messages or idle batches
    nextNetwork(block);

    // The time of the next block
    if (block.m_baud > 0.0) {
        m_nowMs += (int64_t) std::llround((double) block.m_levels.size() * 1000.0 / block.m_baud);
    }

    return true;
}

void RadioPagingPocsagTxStream::nextNetwork(RadioPagingTxBlock& block)
{
    if (m_preambleDue)
    {
        block.m_baud = m_group.first;
        block.m_shaping = m_group.first;
        block.m_levels = levels(preambleBits(), m_group.second);
        m_preambleDue = false;
        m_idleBatches = 0;
        m_batchesSincePreamble = 0;
        return;
    }

    std::deque<Item>& queue = m_queues[m_group];
    std::vector<size_t> due = dueItems(queue);
    Group other;
    bool otherWaiting = nextGroup(other);

    // The turn of a busy group ends when another one waits
    if (!due.empty() && !(otherWaiting && (m_turnBlocks >= m_maxBlocksPerTurn)))
    {
        // The messages due at this batch boundary, in their batches. They were encoded
        // one by one when queued; together they encode as well, checked all the same
        std::vector<RadioPagingPocsag::TxMessage> messages;
        RadioPagingPocsag::EncodeOptions options;
        options.m_preamble = false;

        for (size_t i : due) {
            messages.push_back(queue[i].m_message);
        }

        RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode(messages, options);

        if (!result.m_ok)
        {
            // Sent one by one, the first now
            messages.resize(1);
            due.resize(1);
            result = RadioPagingPocsag::encode(messages, options);
        }

        takeSent(queue, due, result, m_batchesSincePreamble, block);
        m_batchesSincePreamble += result.m_batches;
        block.m_baud = m_group.first;
        block.m_shaping = m_group.first;
        block.m_levels = levels(result.m_bits, m_group.second);
        m_idleBatches = 0;
        m_turnBlocks++;
        return;
    }

    // Another group: when this one is idle long enough or its turn is over; with nothing to
    // send anywhere, back to the default group
    bool anyDue = false;

    for (const auto& q : m_queues) {
        anyDue = anyDue || hasDue(q.second);
    }

    bool turnOver = otherWaiting && (m_turnBlocks >= m_maxBlocksPerTurn);
    bool idle = (m_idleBatches >= m_idleBatchesBeforeSwitch);
    bool toDefault = idle && !otherWaiting && (m_group != m_defaultGroup) && !anyDue;

    if ((idle && otherWaiting) || turnOver || toDefault)
    {
        m_group = (otherWaiting) ? other : m_defaultGroup;
        block.m_baud = m_group.first;
        block.m_shaping = m_group.first;
        block.m_levels = levels(preambleBits(), m_group.second);
        m_idleBatches = 0;
        m_turnBlocks = 0;
        m_batchesSincePreamble = 0;
        return;
    }

    block.m_baud = m_group.first;
    block.m_shaping = m_group.first;
    block.m_levels = levels(idleBatchBits(), m_group.second);
    m_idleBatches++;
    m_batchesSincePreamble++;
    m_turnBlocks = 0;
}

// The turns count over the runs: a group refilled at every run gives way after its blocks
void RadioPagingPocsagTxStream::startRun()
{
    m_preambleDue = true;
}

bool RadioPagingPocsagTxStream::nextMixed(RadioPagingTxBlock& block, int64_t maxTicks,
    const std::function<int64_t(int)>& overhead)
{
    block = RadioPagingTxBlock();
    startDelays();

    Group other;
    bool otherWaiting = nextGroup(other);
    bool currentEmpty = !hasDue(m_queues[m_group]);

    // The group changes when it has nothing to send, or when its turn is over and another waits
    if (currentEmpty || (otherWaiting && (m_turnBlocks >= m_maxBlocksPerTurn)))
    {
        if (!otherWaiting) {
            return false;       // Nothing queued in any group
        }

        m_group = other;
        m_preambleDue = true;
        m_turnBlocks = 0;
    }

    std::deque<Item>& queue = m_queues[m_group];

    if (overhead) {
        maxTicks -= overhead(m_group.first);
    }

    int64_t ticksPerBit = RadioPagingTxBlock::m_tickRate / m_group.first;
    size_t maxBits = (size_t) std::max<int64_t>(0, maxTicks / ticksPerBit);
    RadioPagingPocsag::EncodeOptions options;
    options.m_preamble = m_preambleDue;

    // As many messages due as fit, the first at least
    std::vector<size_t> due = dueItems(queue);
    std::vector<RadioPagingPocsag::TxMessage> messages;
    RadioPagingPocsag::EncodeResult best;
    size_t used = 0;

    for (size_t i : due)
    {
        messages.push_back(queue[i].m_message);
        RadioPagingPocsag::EncodeResult result = RadioPagingPocsag::encode(messages, options);

        if (!result.m_ok || (result.m_bits.size() > maxBits)) {
            break;
        }

        best = result;
        used++;
    }

    if (used == 0) {
        return false;
    }

    due.resize(used);

    // After a preamble the batches count from 0
    if (m_preambleDue) {
        m_batchesSincePreamble = 0;
    }

    takeSent(queue, due, best, m_batchesSincePreamble, block);
    m_batchesSincePreamble += best.m_batches;

    block.m_baud = m_group.first;
    block.m_shaping = m_group.first;
    block.m_levels = levels(best.m_bits, m_group.second);
    m_preambleDue = false;
    m_turnBlocks++;
    m_inTransmission = true;
    return true;
}

// ---------------------------------------------------------------------------
// FLEX
// ---------------------------------------------------------------------------

RadioPagingFlexTxStream::RadioPagingFlexTxStream(bool network, RadioPagingFlex::Mode mode, bool inverted, int collapse) :
    m_network(network),
    m_inverted(inverted),
    m_nextInverted(inverted),
    m_collapse(network ? std::max(0, std::min(RadioPagingFlex::m_maxCollapse, collapse)) : 0),
    m_ersCollapse(std::max(0, std::min(RadioPagingFlex::m_maxCollapse, collapse))),
    m_frame(0),
    m_ersBits(0)
{
    std::string error;

    for (int p = 0; p < 2; p++)
    {
        m_encoders[p].setMode(mode);
        m_encoders[p].setInverted(p == 1);
        m_encoders[p].setCollapse(m_collapse, error);
    }
}

bool RadioPagingFlexTxStream::add(const RadioPagingFlexEncoder::TxMessage& message, bool inverted, uint64_t id,
    std::string& error)
{
    int p = inverted ? 1 : 0;

    // A one-shot transmission ends with its last message: nothing waits for later frames
    if (!m_network && ((message.m_retransmit > 0) || (message.m_sendDelay > 0)))
    {
        error = "Retransmission and send delay need the network mode";
        return false;
    }

    RadioPagingFlexEncoder::EnqueueResult result = m_encoders[p].enqueue(message);

    if (!result.m_ok)
    {
        error = result.m_error;
        return false;
    }

    m_ids[p][result.m_id] = id;
    return true;
}

void RadioPagingFlexTxStream::setMode(RadioPagingFlex::Mode mode)
{
    m_encoders[0].setMode(mode);
    m_encoders[1].setMode(mode);
}

void RadioPagingFlexTxStream::setRoaming(const RadioPagingFlexEncoder::Roaming& roaming)
{
    m_encoders[0].setRoaming(roaming);
    m_encoders[1].setRoaming(roaming);
}

bool RadioPagingFlexTxStream::mandatory(uint64_t frameIndex) const
{
    // A fragment of a message whose first fragment has gone: the next one is in this frame
    return m_encoders[m_inverted ? 1 : 0].mandatory(frameIndex);
}

bool RadioPagingFlexTxStream::setCollapse(int collapse, std::string& error)
{
    if ((collapse < 0) || (collapse > RadioPagingFlex::m_maxCollapse))
    {
        error = "Collapse must be 0 to 7";
        return false;
    }
    m_ersCollapse = collapse;

    // One-shot frames keep collapse 0: the pagers have just been re-synchronized
    if (m_network)
    {
        m_collapse = collapse;
        m_encoders[0].setCollapse(collapse, error);
        m_encoders[1].setCollapse(collapse, error);
    }

    return true;
}

bool RadioPagingFlexTxStream::setScheduleCollapse(int collapse, std::string& error)
{
    if ((collapse < 0) || (collapse > RadioPagingFlex::m_maxCollapse))
    {
        error = "Collapse must be 0 to 7";
        return false;
    }
    if (m_network)
    {
        m_collapse = collapse;
        m_encoders[0].setCollapse(collapse, error);
        m_encoders[1].setCollapse(collapse, error);
    }

    return true;
}

bool RadioPagingFlexTxStream::setTimeService(const RadioPagingFlexEncoder::TimeService& time, std::string& error)
{
    return m_encoders[0].setTimeService(time, error) && m_encoders[1].setTimeService(time, error);
}

bool RadioPagingFlexTxStream::setNumbering(const RadioPagingFlexEncoder::Numbering& numbering, std::string& error)
{
    return m_encoders[0].setNumbering(numbering, error) && m_encoders[1].setNumbering(numbering, error);
}

bool RadioPagingFlexTxStream::notifySystemEvent(unsigned events, int frames, std::string& error)
{
    // Each frame for a cycle: a one-shot transmission does not last that long
    if (!m_network)
    {
        error = "The System Event Notification needs the network mode";
        return false;
    }

    return m_encoders[0].notifySystemEvent(events, frames, error) && m_encoders[1].notifySystemEvent(events, frames, error);
}

void RadioPagingFlexTxStream::cancelSystemEvent()
{
    m_encoders[0].cancelSystemEvent();
    m_encoders[1].cancelSystemEvent();
}

bool RadioPagingFlexTxStream::setReservedFrames(const std::vector<uint8_t>& table, std::string& error)
{
    return m_encoders[0].setReservedFrames(table, error) && m_encoders[1].setReservedFrames(table, error);
}

bool RadioPagingFlexTxStream::due(uint64_t frameIndex) const
{
    return (m_encoders[0].eligible(frameIndex) > 0) || (m_encoders[1].eligible(frameIndex) > 0);
}

bool RadioPagingFlexTxStream::hasWork() const
{
    return m_network || (pending() > 0);
}

void RadioPagingFlexTxStream::plan(int64_t epochMs, int collapse, uint64_t& firstFrame, int& ersBits)
{
    // The first frame starts on the first frame boundary at least the minimum
    // re-synchronization after the start
    int64_t minMs = ((int64_t) RadioPagingFlex::ersMinBits(collapse) * 1000) / RadioPagingFlex::m_syncBaud;
    int64_t earliest = epochMs + minMs;
    int64_t firstMs = ((earliest + RadioPagingFlex::m_frameMs - 1) / RadioPagingFlex::m_frameMs) * RadioPagingFlex::m_frameMs;

    firstFrame = frameIndex(firstMs);
    // Rounded up: the first frame starts on the boundary or within a bit after it, never before
    ersBits = (int) (((firstMs - epochMs) * RadioPagingFlex::m_syncBaud + 999) / 1000);
}

void RadioPagingFlexTxStream::begin(int64_t epochMs)
{
    plan(epochMs, m_ersCollapse, m_frame, m_ersBits);
    m_inverted = m_nextInverted;
    m_blocks.clear();
}

size_t RadioPagingFlexTxStream::pending() const
{
    return m_encoders[0].pending() + m_encoders[1].pending();
}

std::vector<uint64_t> RadioPagingFlexTxStream::clear()
{
    std::vector<uint64_t> ids;

    for (int p = 0; p < 2; p++)
    {
        for (const auto& entry : m_ids[p])
        {
            // A message sent already and waiting for a retransmission is not reported again
            if (m_encoders[p].remove(entry.first) && (m_sent[p].count(entry.first) == 0)) {
                ids.push_back(entry.second);
            }
        }
        m_ids[p].clear();
        m_sent[p].clear();
    }

    return ids;
}

// The rest of the frame being sent: its messages were taken from the encoders
std::vector<uint64_t> RadioPagingFlexTxStream::cut()
{
    std::vector<uint64_t> ids;

    for (const RadioPagingTxBlock& block : m_blocks) {
        ids.insert(ids.end(), block.m_ids.begin(), block.m_ids.end());
    }

    m_blocks.clear();
    m_ersBits = 0;
    return ids;
}

std::vector<float> RadioPagingFlexTxStream::levels(const RadioPagingFlex::SymbolBlock& block)
{
    static const float levels4[4] = {-1.0f, -1.0f / 3.0f, 1.0f / 3.0f, 1.0f};
    std::vector<float> out;
    out.reserve(block.m_symbols.size());

    for (uint8_t s : block.m_symbols) {
        out.push_back((block.m_levels == 4) ? levels4[s & 3] : ((s & 1) ? 1.0f : -1.0f));
    }

    return out;
}

// The frame of m_frame with the messages due in it: in the network polarity, or in the
// other one when it has messages due and the network has nothing mandatory in the frame
void RadioPagingFlexTxStream::buildFrame()
{
    int network = m_inverted ? 1 : 0;
    int other = 1 - network;
    int p = ((m_encoders[other].eligible(m_frame) > 0) && !mandatory(m_frame)) ? other : network;
    RadioPagingFlexEncoder::Frame frame = m_encoders[p].buildFrame(m_frame);
    std::vector<uint64_t> ids;
    std::vector<uint64_t> repeatIds;
    std::vector<RadioPagingFlexEncoder::SentStatus> status;
    std::vector<RadioPagingFlexEncoder::SentStatus> repeatStatus;

    for (size_t i = 0; i < frame.m_sentIds.size(); i++)
    {
        auto it = m_ids[p].find(frame.m_sentIds[i]);

        if (it != m_ids[p].end())
        {
            ids.push_back(it->second);
            status.push_back((i < frame.m_sentStatus.size()) ? frame.m_sentStatus[i] : RadioPagingFlexEncoder::SentStatus());
        }
    }
    for (size_t i = 0; i < frame.m_retransmittedIds.size(); i++)
    {
        auto it = m_ids[p].find(frame.m_retransmittedIds[i]);

        if (it != m_ids[p].end())
        {
            repeatIds.push_back(it->second);
            repeatStatus.push_back((i < frame.m_retransmittedStatus.size()) ? frame.m_retransmittedStatus[i]
                : RadioPagingFlexEncoder::SentStatus());
        }
    }

    // A message to retransmit keeps its id until its last retransmission (clear() removes what is left of it)
    std::vector<uint64_t> failedIds;

    for (uint32_t encoderId : frame.m_failedIds)
    {
        auto it = m_ids[p].find(encoderId);

        if (it != m_ids[p].end())
        {
            failedIds.push_back(it->second);
            m_ids[p].erase(it);
        }
    }
    for (uint32_t encoderId : frame.m_doneIds)
    {
        m_ids[p].erase(encoderId);
        m_sent[p].erase(encoderId);
    }
    for (uint32_t encoderId : frame.m_sentIds)
    {
        if (m_ids[p].count(encoderId) > 0) {
            m_sent[p].insert(encoderId);
        }
    }

    m_frame++;

    for (size_t i = 0; i < frame.m_blocks.size(); i++)
    {
        RadioPagingTxBlock block;
        block.m_baud = frame.m_blocks[i].m_baud;
        block.m_shaping = m_shaping;
        block.m_levels = levels(frame.m_blocks[i]);

        if (i + 1 == frame.m_blocks.size())
        {
            block.m_ids = ids;
            block.m_repeatIds = repeatIds;
            block.m_status = status;
            block.m_repeatStatus = repeatStatus;
            block.m_failedIds = failedIds;
        }

        m_blocks.push_back(block);
    }

    // A reserved frame has no blocks: silence would break the frame timing, the time is
    // filled with re-synchronization
    if (frame.m_blocks.empty())
    {
        RadioPagingTxBlock block;
        block.m_baud = RadioPagingFlex::m_syncBaud;
        block.m_shaping = m_shaping;
        block.m_levels = levels(RadioPagingFlex::ers(RadioPagingFlex::m_bitsPerFrame, m_inverted));
        block.m_failedIds = failedIds;
        m_blocks.push_back(block);
    }
}

bool RadioPagingFlexTxStream::next(RadioPagingTxBlock& block)
{
    block = RadioPagingTxBlock();

    if (m_ersBits > 0)
    {
        block.m_baud = RadioPagingFlex::m_syncBaud;
        block.m_shaping = m_shaping;
        block.m_levels = levels(RadioPagingFlex::ers(m_ersBits, m_inverted));
        block.m_ers = true;
        m_ersBits = 0;
        return true;
    }

    if (m_blocks.empty())
    {
        if (!m_network && (pending() == 0)) {
            return false;
        }

        buildFrame();
    }

    block = m_blocks.front();
    m_blocks.pop_front();
    return true;
}

// ---------------------------------------------------------------------------
// GSC
// ---------------------------------------------------------------------------

const int RadioPagingGscTxStream::m_baud;
const int RadioPagingGscTxStream::m_alertMs;

RadioPagingGscTxStream::RadioPagingGscTxStream(RadioPagingGsc::Mode mode) :
    m_mode(mode),
    m_inTransmission(false)
{
}

bool RadioPagingGscTxStream::add(const RadioPagingGsc::TxMessage& message, bool inverted, uint64_t id, std::string& error,
    const Voice& voice)
{
    if (message.m_type == RadioPagingGsc::Data)
    {
        error = "GSC message type must be tone, voice, numeric or alphanumeric";
        return false;
    }
    if ((message.m_type == RadioPagingGsc::Voice) && (!voice || voice->m_samples.empty()))
    {
        error = "A GSC voice page needs a sound file";
        return false;
    }

    RadioPagingGsc::EncodeOptions options;
    options.m_mode = RadioPagingGsc::Single;
    RadioPagingGsc::EncodeResult trial = RadioPagingGsc::encode({ message }, options);

    if (!trial.m_ok)
    {
        error = trial.m_error;
        return false;
    }

    m_queue.push_back(Item{message, inverted, trial.m_preamble, id, (message.m_type == RadioPagingGsc::Voice) ? voice : Voice()});
    return true;
}

bool RadioPagingGscTxStream::addLive(const RadioPagingGsc::TxMessage& message, bool inverted, uint64_t id, std::string& error,
    int maxSeconds)
{
    if (message.m_type != RadioPagingGsc::Voice)
    {
        error = "A live page is a voice page";
        return false;
    }
    if (maxSeconds < 1)
    {
        error = "A live voice page needs a voice length";
        return false;
    }

    RadioPagingGsc::EncodeOptions options;
    options.m_mode = RadioPagingGsc::Single;
    RadioPagingGsc::EncodeResult trial = RadioPagingGsc::encode({ message }, options);

    if (!trial.m_ok)
    {
        error = trial.m_error;
        return false;
    }

    Item item{message, inverted, trial.m_preamble, id, Voice()};
    item.m_live = true;
    item.m_liveSeconds = maxSeconds;
    m_queue.push_back(item);
    return true;
}

void RadioPagingGscTxStream::pushLive(const float *samples, size_t count)
{
    // About a second of voice at most waits: the voice is sent as it comes
    const size_t maxWaiting = RadioPagingWav::m_sampleRate;

    if ((m_live != LiveVoice) || m_liveEnding) {
        return;
    }

    for (size_t i = 0; (i < count) && (m_liveSamples.size() < maxWaiting); i++) {
        m_liveSamples.push_back(samples[i]);
    }
}

void RadioPagingGscTxStream::endLive()
{
    if (m_live != LiveNone) {
        m_liveEnding = true;
    }
}

// The live page: the voice pushed, then the turn-off word. With no voice waiting the carrier is
// left unmodulated for 10 ms at a time. The voice ends at the longest voice length, counted in
// time (what is waiting is sent first)
bool RadioPagingGscTxStream::nextLive(RadioPagingTxBlock& block)
{
    const size_t chunk = RadioPagingWav::m_sampleRate / 40;     // 25 ms
    const size_t silence = RadioPagingWav::m_sampleRate / 100;  // 10 ms

    if (m_live == LiveAlert)
    {
        m_live = LiveVoice;
        m_liveSent = 0;
    }

    bool limit = (m_liveSent >= m_liveLimit);

    if (limit || (m_liveEnding && m_liveSamples.empty()))
    {
        block = m_liveEnd;
        m_live = LiveNone;
        m_liveEnding = false;
        m_liveSamples.clear();
        return true;
    }

    block.m_baud = RadioPagingWav::m_sampleRate;
    block.m_shaping = RadioPagingWav::m_sampleRate / 2.0;

    if (m_liveSamples.empty())
    {
        block.m_levels.assign(silence, 0.0f);
        m_liveSent += silence;
        return true;
    }

    size_t n = std::min(chunk, m_liveSamples.size());

    for (size_t i = 0; i < n; i++)
    {
        float sample = m_liveProcessor.process(m_liveSamples.front());
        block.m_levels.push_back(m_liveInverted ? -sample : sample);
        m_liveSamples.pop_front();
    }

    m_liveSent += n;
    return true;
}

void RadioPagingGscTxStream::begin(int64_t epochMs)
{
    (void) epochMs;
    m_inTransmission = false;
}

// The messages waiting, and the ones taken from the queue that could not be encoded. The
// transmission being sent goes on
std::vector<uint64_t> RadioPagingGscTxStream::clear()
{
    std::vector<uint64_t> ids;

    ids.swap(m_failed);

    for (const Item& item : m_queue) {
        ids.push_back(item.m_id);
    }

    m_queue.clear();
    return ids;
}

// The transmission is cut: the messages of the blocks not returned yet (the end of a voice page)
// and the ones that could not be encoded
std::vector<uint64_t> RadioPagingGscTxStream::cut()
{
    std::vector<uint64_t> ids;

    ids.swap(m_failed);

    for (const RadioPagingTxBlock& block : m_blocks) {
        ids.insert(ids.end(), block.m_ids.begin(), block.m_ids.end());
    }

    m_blocks.clear();

    // A live page: its id is on the turn-off word, not sent yet
    if (m_live != LiveNone)
    {
        ids.push_back(m_liveId);
        m_live = LiveNone;
        m_liveEnding = false;
        m_liveSamples.clear();
    }

    return ids;
}

// One transmission: the first message queued and the ones that go with it in the mode. A voice
// page is a transmission of its own: the page, the alert time and the voice, the turn-off word
bool RadioPagingGscTxStream::next(RadioPagingTxBlock& block)
{
    block = RadioPagingTxBlock();

    if (!m_blocks.empty())
    {
        block = m_blocks.front();
        m_blocks.pop_front();
        return true;
    }

    if (m_live != LiveNone) {
        return nextLive(block);
    }

    if (m_inTransmission || m_queue.empty())
    {
        m_inTransmission = false;
        return false;
    }

    const Item first = m_queue.front();
    const bool voicePage = (first.m_message.m_type == RadioPagingGsc::Voice);
    std::vector<Item> group;
    bool together = (m_mode != RadioPagingGsc::Single) && !voicePage;

    for (auto it = m_queue.begin(); (it != m_queue.end()) && (group.size() < (size_t) RadioPagingGsc::m_maxBatchMessages);)
    {
        bool take = group.empty()
            || (together && (it->m_message.m_type != RadioPagingGsc::Voice) && (it->m_inverted == first.m_inverted)
                && ((m_mode == RadioPagingGsc::NonBatterySaver) || (it->m_preamble == first.m_preamble)));

        if (take)
        {
            group.push_back(*it);
            it = m_queue.erase(it);
        }
        else
        {
            ++it;
        }
    }

    RadioPagingGsc::EncodeOptions options;
    options.m_mode = voicePage ? RadioPagingGsc::Single : m_mode;
    std::vector<RadioPagingGsc::TxMessage> messages;

    for (const Item& item : group) {
        messages.push_back(item.m_message);
    }

    RadioPagingGsc::EncodeResult result = RadioPagingGsc::encode(messages, options);

    // The messages were encoded one by one when queued: if the group does not encode, the
    // first goes alone and the others stay queued, in their order
    if (!result.m_ok && (group.size() > 1))
    {
        for (size_t i = group.size() - 1; i > 0; i--) {
            m_queue.push_front(group[i]);
        }

        group.resize(1);
        options.m_mode = RadioPagingGsc::Single;
        result = RadioPagingGsc::encode({ group[0].m_message }, options);
    }

    // Not reached: the message was encoded when queued. If it ever happens the ids are reported
    // with the messages dropped (cut, clear) and the transmission ends
    if (!result.m_ok)
    {
        for (const Item& item : group) {
            m_failed.push_back(item.m_id);
        }

        return false;
    }

    auto symbols = [&first](const std::vector<uint8_t>& bits) {
        std::vector<float> levels;

        for (uint8_t bit : bits) {
            levels.push_back((bit != 0) != first.m_inverted ? 1.0f : -1.0f);
        }

        return levels;
    };

    block.m_baud = m_baud;
    block.m_shaping = m_baud;
    block.m_levels = symbols(result.m_bits);

    // Where each page went
    auto gscStatus = [&result](size_t index) {
        RadioPagingFlexEncoder::SentStatus status;
        status.m_preamble = result.m_preamble;
        status.m_batch = result.m_batch ? 1 : 0;

        if (index < result.m_messageSlots.size())
        {
            status.m_slot = result.m_messageSlots[index];
            status.m_blocks = result.m_messageBlocks[index];
        }

        return status;
    };

    if (!voicePage)
    {
        for (size_t i = 0; i < group.size(); i++)
        {
            block.m_ids.push_back(group[i].m_id);
            block.m_status.push_back(gscStatus(i));
        }
    }
    else if (first.m_live)
    {
        block.m_startIds.push_back(first.m_id);     // On air with the page; sent with its turn-off word
        // The alert time, then the voice as it comes (nextLive); the turn-off word is prepared with
        // the id of the page
        RadioPagingTxBlock alert;
        alert.m_baud = RadioPagingWav::m_sampleRate;
        alert.m_shaping = RadioPagingWav::m_sampleRate / 2.0;
        alert.m_levels.assign((size_t) (m_alertMs * RadioPagingWav::m_sampleRate / 1000), 0.0f);

        std::vector<uint8_t> turnOff;
        RadioPagingGsc::appendActivationCode(turnOff);
        m_liveEnd = RadioPagingTxBlock();
        m_liveEnd.m_baud = m_baud;
        m_liveEnd.m_shaping = m_baud;
        m_liveEnd.m_levels = symbols(turnOff);
        m_liveEnd.m_ids.push_back(first.m_id);
        m_liveEnd.m_status.push_back(gscStatus(0));

        m_blocks.push_back(alert);
        m_live = LiveAlert;
        m_liveId = first.m_id;
        m_liveInverted = first.m_inverted;
        m_liveEnding = false;
        m_liveLimit = (size_t) first.m_liveSeconds * RadioPagingWav::m_sampleRate;
        m_liveSent = 0;
        m_liveSamples.clear();
        m_liveProcessor.reset();
    }
    else
    {
        block.m_startIds.push_back(first.m_id);     // On air with the page; sent with its turn-off word
        // The alert time (the carrier unmodulated) and the voice, as they are; inverted with the
        // polarity of the transmission. Then the activation code again after a comma: the
        // turn-off word, with the id of the page
        RadioPagingTxBlock audio;
        const RadioPagingWav::Audio& voice = *first.m_voice;
        size_t alert = (size_t) (m_alertMs * voice.m_rate / 1000.0);

        RadioPagingVoiceProcessor processor((int) voice.m_rate);

        audio.m_baud = voice.m_rate;
        audio.m_shaping = voice.m_rate / 2.0;
        audio.m_levels.assign(alert, 0.0f);

        // The voice processed, and the samples still in the processor at its end
        for (size_t i = 0; i < voice.m_samples.size() + (size_t) processor.delay(); i++)
        {
            float sample = processor.process((i < voice.m_samples.size()) ? voice.m_samples[i] : 0.0f);

            if (i >= (size_t) processor.delay()) {
                audio.m_levels.push_back(first.m_inverted ? -sample : sample);
            }
        }

        std::vector<uint8_t> turnOff;
        RadioPagingGsc::appendActivationCode(turnOff);

        RadioPagingTxBlock end;
        end.m_baud = m_baud;
        end.m_shaping = m_baud;
        end.m_levels = symbols(turnOff);
        end.m_ids.push_back(first.m_id);
        end.m_status.push_back(gscStatus(0));

        m_blocks.push_back(audio);
        m_blocks.push_back(end);
    }

    m_inTransmission = true;
    return true;
}

// ---------------------------------------------------------------------------
// FLEX mixed with POCSAG
// ---------------------------------------------------------------------------

const int64_t RadioPagingMixTxStream::m_frameTicks;
const int RadioPagingMixTxStream::m_minuteFrames;
const int RadioPagingMixTxStream::m_switchBaud;
const int RadioPagingMixTxStream::m_fillBaud;

RadioPagingMixTxStream::RadioPagingMixTxStream(const Config& config) :
    m_config(config),
    m_flex(true, config.m_flexMode, config.m_flexInverted, config.m_collapse),
    m_pocsag(true, config.m_pocsagBaud, config.m_pocsagInverted),
    m_ersPending(false),
    m_flexFrame(false),
    m_lastFlex(true),
    m_frameIdx(0),
    m_toBoundary(0),
    m_hasPending(false)
{
    std::string error;
    setConfig(config, error);
}

bool RadioPagingMixTxStream::setConfig(const Config& config, std::string& error)
{
    Config c = config;
    int period = c.m_shared ? 4 * m_minuteFrames : m_minuteFrames;

    c.m_collapse = std::max(0, std::min(RadioPagingFlex::m_maxCollapse, c.m_collapse));
    c.m_framesPerMinute = std::max(0, std::min(period, c.m_framesPerMinute));
    m_config = c;

    m_flex.setMode(c.m_flexMode);
    m_flex.setInverted(c.m_flexInverted);
    m_flex.setRoaming(c.m_roaming);

    if (!m_flex.setTimeService(c.m_time, error)) {
        return false;
    }
    if (!m_flex.setNumbering(c.m_numbering, error)) {
        return false;
    }

    // 7.1.2: N frames a minute, the system collapse of the minute (5, 7 for 4 minutes), which
    // is also what the re-synchronization has to reach; the capcodes of the other frames can
    // not be assigned. 7.1.1: the collapse and no reservation
    std::vector<uint8_t> table;
    int schedule = c.m_collapse;

    if (c.m_framesPerMinute > 0) {
        schedule = c.m_shared ? 7 : 5;
    }

    if (!m_flex.setCollapse(std::max(c.m_collapse, schedule), error)) {
        return false;
    }

    if (c.m_framesPerMinute > 0)
    {
        table.assign(RadioPagingFlexScheduler::m_framesPerCycle, 0);

        for (int f = RadioPagingFlexScheduler::m_protectedFrames; f < RadioPagingFlexScheduler::m_framesPerCycle; f++) {
            table[f] = ((f % period) >= c.m_framesPerMinute) ? 1 : 0;
        }
    }

    if (!m_flex.setScheduleCollapse(schedule, error) || !m_flex.setReservedFrames(table, error)) {
        return false;
    }

    m_reserved = table;
    m_pocsag.setDefaultGroup(c.m_pocsagBaud, c.m_pocsagInverted);
    return true;
}

bool RadioPagingMixTxStream::addFlex(const RadioPagingFlexEncoder::TxMessage& message, bool inverted, uint64_t id,
    std::string& error)
{
    // 7.1.2: a pager listens to the frames of its class (the system collapse of the minute):
    // the class must be one of the frames transmitted. Frames 0-3 are sent whatever the plan,
    // but a pager of the class 2 or 3 would see one frame in 4 minutes (7.1.2 example)
    if (m_config.m_framesPerMinute > 0)
    {
        uint32_t period = m_config.m_shared ? 4 * m_minuteFrames : m_minuteFrames;

        if (RadioPagingFlexScheduler::capcodeSlot(message.m_capcode).m_frame % period >= (uint32_t) m_config.m_framesPerMinute)
        {
            error = "The capcode listens to frames that are not transmitted (FLEX frames per minute)";
            return false;
        }
    }

    return m_flex.add(message, inverted, id, error);
}

bool RadioPagingMixTxStream::addPocsag(const RadioPagingPocsag::TxMessage& message, int baud, bool inverted,
    uint64_t id, std::string& error, const RadioPagingPocsagTxStream::Schedule& schedule)
{
    RadioPagingPocsag::EncodeResult trial = RadioPagingPocsag::encode({ message });

    if (!trial.m_ok)
    {
        error = trial.m_error;
        return false;
    }

    // The message has to fit the longest gap FLEX can leave: pattern, preamble and batches
    int64_t gap = longestGapTicks();
    int64_t needed = (int64_t) trial.m_bits.size() * (RadioPagingTxBlock::m_tickRate / baud) + switchTicks(baud);

    if (gap == 0)
    {
        error = "The FLEX plan (collapse, frames per minute) leaves no time for POCSAG";
        return false;
    }
    if (needed > gap)
    {
        error = "The message is longer than the longest gap FLEX leaves to POCSAG";
        return false;
    }

    return m_pocsag.add(message, baud, inverted, id, error, schedule);
}

void RadioPagingMixTxStream::begin(int64_t epochMs)
{
    m_flex.begin(epochMs);
    m_pocsag.begin(epochMs);
    m_ersPending = true;
    m_flexFrame = false;
    m_hasPending = false;
    m_lastFlex = true;
    m_frameIdx = m_flex.currentFrame();
    m_toBoundary = 0;
}

size_t RadioPagingMixTxStream::pending() const
{
    return m_flex.pending() + m_pocsag.pending() + (m_hasPending ? m_pendingBlock.m_ids.size() : 0);
}

std::vector<uint64_t> RadioPagingMixTxStream::clear()
{
    std::vector<uint64_t> ids = m_flex.clear();
    std::vector<uint64_t> more = m_pocsag.clear();

    ids.insert(ids.end(), more.begin(), more.end());

    // The POCSAG block behind the switch pattern is dropped, its time is kept: the FLEX
    // frame after it stays on the grid
    if (m_hasPending)
    {
        ids.insert(ids.end(), m_pendingBlock.m_ids.begin(), m_pendingBlock.m_ids.end());
        m_pendingBlock = fillBlock(m_pendingBlock.ticks());
    }

    return ids;
}

// The blocks prepared for the transmission cut: the POCSAG block waiting behind its switch
// pattern and the rest of the FLEX frame
std::vector<uint64_t> RadioPagingMixTxStream::cut()
{
    std::vector<uint64_t> ids = m_flex.cut();

    if (m_hasPending) {
        ids.insert(ids.end(), m_pendingBlock.m_ids.begin(), m_pendingBlock.m_ids.end());
    }

    m_hasPending = false;
    m_flexFrame = false;
    m_ersPending = false;
    return ids;
}

bool RadioPagingMixTxStream::reserved(uint64_t frameIndex) const
{
    return !m_reserved.empty() && (m_reserved[RadioPagingFlexScheduler::frameTime(frameIndex).m_frame] != 0);
}

bool RadioPagingMixTxStream::required(uint64_t frameIndex) const
{
    uint32_t frame = RadioPagingFlexScheduler::frameTime(frameIndex).m_frame;

    // The mandatory frames of roaming are never mixed (Motorola 3.9.1 rules 5 and 9)
    if (frame < (uint32_t) RadioPagingFlexScheduler::m_protectedFrames) {
        return true;
    }
    if (m_flex.due(frameIndex)) {
        return true;
    }

    return minimumActivity(frame);
}

// The frame is one of the minimum activity of the plan (7.1.1 or 7.1.2)
bool RadioPagingMixTxStream::minimumActivity(uint32_t frame) const
{
    uint32_t period = m_config.m_shared ? 4 * m_minuteFrames : m_minuteFrames;
    uint32_t minimum = (m_config.m_framesPerMinute > 0) ? (uint32_t) m_config.m_framesPerMinute : (1u << m_config.m_collapse);
    return (frame % period) < minimum;
}

// The longest time FLEX leaves to POCSAG when no FLEX message is queued: the longest run of
// frames of the cycle that are neither mandatory nor minimum activity
int64_t RadioPagingMixTxStream::longestGapTicks() const
{
    int run = 0;
    int longest = 0;

    for (uint32_t frame = 0; frame < (uint32_t) RadioPagingFlexScheduler::m_framesPerCycle; frame++)
    {
        if ((frame < (uint32_t) RadioPagingFlexScheduler::m_protectedFrames) || minimumActivity(frame)) {
            run = 0;
        } else {
            longest = std::max(longest, ++run);
        }
    }

    return (int64_t) longest * m_frameTicks;
}

// Time from now to the start of the next required frame
int64_t RadioPagingMixTxStream::availableTicks() const
{
    uint64_t k = m_frameIdx;

    for (int guard = 0; (guard < RadioPagingFlexScheduler::m_framesPerCycle) && !required(k); guard++) {
        k++;
    }

    return m_toBoundary + (int64_t) (k - m_frameIdx) * m_frameTicks;
}

// The time of ticks has passed: the frame and the time to its boundary
void RadioPagingMixTxStream::advance(int64_t ticks)
{
    int64_t remaining = ticks;

    if (m_toBoundary > 0)
    {
        if (ticks < m_toBoundary)
        {
            m_toBoundary -= ticks;
            return;
        }

        remaining = ticks - m_toBoundary;   // The frame m_frameIdx starts
    }

    m_frameIdx += (uint64_t) (remaining / m_frameTicks);
    int64_t inside = remaining % m_frameTicks;

    if (inside > 0)
    {
        m_frameIdx++;
        m_toBoundary = m_frameTicks - inside;
    }
    else
    {
        m_toBoundary = 0;
    }
}

int64_t RadioPagingMixTxStream::switchTicks(int baud) const
{
    int index = (baud == 512) ? 0 : (baud == 1200) ? 1 : 2;
    // At least the duration set, in groups of 5 bits (6.67 ms) so that the blocks are a whole
    // number of samples (5 ticks) at the internal rate
    int64_t bits = ((int64_t) m_config.m_switchMs[index] * m_switchBaud + 999) / 1000;
    bits = ((bits + 4) / 5) * 5;
    return bits * (RadioPagingTxBlock::m_tickRate / m_switchBaud);
}

// The 1,0 pattern at 750 bps in the polarity of the group, before its preamble
RadioPagingTxBlock RadioPagingMixTxStream::switchBlock(int baud, bool inverted) const
{
    RadioPagingTxBlock block;
    int64_t bits = switchTicks(baud) / (RadioPagingTxBlock::m_tickRate / m_switchBaud);
    std::vector<uint8_t> pattern;

    for (int64_t i = 0; i < bits; i++) {
        pattern.push_back((i & 1) ? 0 : 1);
    }

    block.m_baud = m_switchBaud;
    block.m_shaping = m_switchBaud;
    block.m_deviation = m_config.m_pocsagDeviation;
    block.m_levels = RadioPagingPocsagTxStream::levels(pattern, inverted);
    return block;
}

// 1,0 at about 1600 bps for exactly the ticks: the rate is the one that makes whole bits fit,
// so the FLEX frame after it starts on the grid. Nothing is left over: a block is at least
// a sample (5 ticks) long, the blocks of the stream are whole samples
RadioPagingTxBlock RadioPagingMixTxStream::fillBlock(int64_t ticks) const
{
    RadioPagingTxBlock block;

    if (ticks < 5) {
        return block;
    }

    int64_t bits = std::max<int64_t>(1, ticks / (RadioPagingTxBlock::m_tickRate / m_fillBaud));

    block.m_baud = (double) bits * RadioPagingTxBlock::m_tickRate / (double) ticks;
    block.m_shaping = RadioPagingFlexTxStream::m_shaping;
    block.m_deviation = m_config.m_flexDeviation;

    for (int64_t i = 0; i < bits; i++) {
        block.m_levels.push_back((i & 1) ? -1.0f : 1.0f);
    }

    return block;
}

// POCSAG from now, if it has messages and a block fits before the next required frame
bool RadioPagingMixTxStream::nextPocsag(RadioPagingTxBlock& block)
{
    if (m_pocsag.pending() == 0) {
        return false;
    }

    int64_t available = availableTicks();

    if (available <= 0) {
        return false;
    }

    bool start = m_lastFlex;

    if (start) {
        m_pocsag.startRun();
    }

    RadioPagingTxBlock data;
    auto overhead = [this, start](int baud) { return start ? switchTicks(baud) : (int64_t) 0; };

    // The time of the block: the frame starting at the next boundary less the time to it
    m_pocsag.setTime((int64_t) m_frameIdx * RadioPagingFlex::m_frameMs - (m_toBoundary * 1000) / RadioPagingTxBlock::m_tickRate);

    if (!m_pocsag.nextMixed(data, available, overhead)) {
        return false;
    }

    data.m_deviation = m_config.m_pocsagDeviation;
    int64_t total = data.ticks();

    if (start)
    {
        block = switchBlock(m_pocsag.groupBaud(), m_pocsag.groupInverted());
        total += block.ticks();
        m_pendingBlock = data;
        m_hasPending = true;
    }
    else
    {
        block = data;
    }

    advance(total);
    m_lastFlex = false;
    return true;
}

// The FLEX frame starting now: the first of its blocks. A frame not transmitted is filled
void RadioPagingMixTxStream::nextFlex(RadioPagingTxBlock& block)
{
    uint64_t frame = m_frameIdx;

    m_lastFlex = true;

    if (reserved(frame) && !required(frame))
    {
        block = fillBlock(m_frameTicks);
        advance(m_frameTicks);
        return;
    }

    m_flex.setFrame(frame);
    m_flex.next(block);
    block.m_deviation = m_config.m_flexDeviation;
    m_flexFrame = m_flex.frameInProgress();
    advance(m_frameTicks);
}

bool RadioPagingMixTxStream::next(RadioPagingTxBlock& block)
{
    block = RadioPagingTxBlock();

    if (m_hasPending)
    {
        block = m_pendingBlock;
        m_hasPending = false;
        return true;
    }

    if (m_flexFrame)
    {
        m_flex.next(block);
        block.m_deviation = m_config.m_flexDeviation;
        m_flexFrame = m_flex.frameInProgress();
        return true;
    }

    if (m_ersPending)
    {
        m_flex.next(block);
        block.m_deviation = m_config.m_flexDeviation;
        m_ersPending = false;
        m_frameIdx = m_flex.currentFrame();
        m_toBoundary = 0;
        m_lastFlex = true;
        return true;
    }

    for (;;)
    {
        if (m_toBoundary > 0)
        {
            // Inside a frame after POCSAG: more POCSAG, or the pattern up to the boundary
            if (nextPocsag(block)) {
                return true;
            }

            block = fillBlock(m_toBoundary);

            if (!block.m_levels.empty())
            {
                advance(m_toBoundary);
                m_lastFlex = true;
                return true;
            }

            // Less than a sample left (blocks are whole samples: not reached)
            m_toBoundary = 0;
            continue;
        }

        if (nextPocsag(block)) {
            return true;
        }

        nextFlex(block);
        return true;
    }
}
