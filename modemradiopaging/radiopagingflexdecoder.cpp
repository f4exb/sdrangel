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

#include "radiopagingflexdecoder.h"

#include <algorithm>
#include <utility>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "radiopagingbch.h"

const int RadioPagingFlexDecoder::m_reassemblySlots;
const int RadioPagingFlexDecoder::m_reassemblyMaxLength;
const int RadioPagingFlexDecoder::m_reassemblyTimeout;
const int RadioPagingFlexDecoder::m_historySize;
const int RadioPagingFlexDecoder::m_historyWindow;
const int RadioPagingFlexDecoder::m_tempSlots;
const int RadioPagingFlexDecoder::m_tempTimeout;
const int RadioPagingFlexDecoder::m_voteThreshold;
const int RadioPagingFlexDecoder::m_voteRing;

namespace {

const int wordsPerFrame = RadioPagingFlex::m_wordsPerFrame;
const unsigned framesPerHour = 1920;

int subframeWords(int transmissions)
{
    switch (transmissions)
    {
    case 2: return 44;
    case 3: return 29;
    case 4: return 22;
    default: return wordsPerFrame;
    }
}

} // namespace

RadioPagingFlexDecoder::RadioPagingFlexDecoder() :
    m_reportFragments(false),
    m_fixedYear(false),
    m_referenceYear(2026),
    m_name('A'),
    m_out(nullptr),
    m_block(nullptr)
{
    reset();
}

void RadioPagingFlexDecoder::reset()
{
    m_status = Status();
    m_report = FrameReport();
    m_ctx = FrameContext();
    m_out = nullptr;
    m_block = nullptr;

    for (int p = 0; p < 2; p++)
    {
        m_pol[p] = PolarityState();
        m_pol[p].m_reassembly.assign(m_reassemblySlots, Reassembly());
        m_pol[p].m_history.assign(m_historySize, HistoryEntry());
    }

    m_historyMatch = nullptr;
}

const char *RadioPagingFlexDecoder::addressTypeName(AddressType type)
{
    switch (type)
    {
    case AddressShort: return "short";
    case AddressLong: return "long";
    case AddressNetwork: return "network";
    case AddressTemporary: return "temporary";
    case AddressOperator: return "operator";
    case AddressInfoService: return "info_service";
    case AddressReserved: return "reserved";
    case AddressUnknown: return "unknown";
    }

    return "unknown";
}

const char *RadioPagingFlexDecoder::fragmentKindName(FragmentKind kind)
{
    switch (kind)
    {
    case FragmentComplete: return "complete";
    case FragmentFirst: return "first";
    case FragmentMiddle: return "middle";
    case FragmentLast: return "last";
    case FragmentUnknown: return "unknown";
    }

    return "unknown";
}

const char *RadioPagingFlexDecoder::messageTypeName(MessageType type)
{
    switch (type)
    {
    case TypeUnknown: return "unknown";
    case TypeTone: return "tone";
    case TypeAlphanumeric: return "alphanumeric";
    case TypeSecure: return "secure";
    case TypeNumeric: return "numeric";
    case TypeSpecialNumeric: return "special_numeric";
    case TypeNumberedNumeric: return "numbered_numeric";
    case TypeNumberedSpecialNumeric: return "numbered_special_numeric";
    case TypeBinary: return "binary";
    case TypeShortMessage: return "short_message";
    case TypeShortInstruction: return "short_instruction";
    }

    return "unknown";
}

// Codeword decoding (RadioPagingBCH::decode): up to two bit errors are corrected, the parity bit counted:
// two in bits 31-1, or one there and the parity bit. A wrong parity after correcting two bits in bits
// 31-1 means three or more errors: rejected
RadioPagingFlexDecoder::Word RadioPagingFlexDecoder::decodeWord(uint32_t raw, bool erased)
{
    Word word;

    word.m_status = WordUncorrectable;

    if (erased) {
        return word;
    }

    // Kept for the text of a word that cannot be corrected: shown as received, marked uncertain
    word.m_received = RadioPagingFlex::wordData(raw);
    word.m_hasReceived = true;

    RadioPagingBCH::Result result = RadioPagingBCH::decode(raw);

    if (result.m_bitErrors < 0) {
        return word;
    }

    if (result.m_bitErrors == 0)
    {
        if (result.m_parityError)
        {
            word.m_status = WordCorrected;
            word.m_bits = 1;
            word.m_parity = true;
        }
        else
        {
            word.m_status = WordClean;
        }
    }
    else
    {
        word.m_status = WordCorrected;
        word.m_bits = (uint8_t) (result.m_bitErrors + (result.m_parityError ? 1 : 0));
    }

    word.m_data = RadioPagingFlex::wordData(result.m_word);
    return word;
}

// Soft decision for a word BCH cannot correct (Chase): each of its three least reliable bits flipped in turn, the
// word BCH corrects that to is a candidate; the candidate whose changed bits have the smallest summed reliability is
// taken, none when two different codewords tie. reliability: of bits 0-31 of raw
bool RadioPagingFlexDecoder::softDecode(uint32_t raw, const uint8_t reliability[32], uint32_t& codeword)
{
    int order[32];

    for (int b = 0; b < 32; b++) {
        order[b] = b;
    }

    // The least reliable bits first, the earlier first among equals
    std::partial_sort(order, order + 3, order + 32, [&](int a, int b) {
        return std::make_pair(reliability[a], a) < std::make_pair(reliability[b], b);
    });

    bool found = false;
    bool tie = false;
    int bestMetric = 0;

    for (int k = 0; k < 3; k++)
    {
        RadioPagingBCH::Result result = RadioPagingBCH::decode(raw ^ (1u << order[k]));

        if (result.m_bitErrors < 0) {
            continue;
        }

        uint32_t changed = result.m_word ^ raw;
        int metric = 0;

        for (int b = 0; b < 32; b++)
        {
            if ((changed >> b) & 1) {
                metric += reliability[b];
            }
        }

        if (!found || (metric < bestMetric))
        {
            found = true;
            tie = false;
            bestMetric = metric;
            codeword = result.m_word;
        }
        else if ((metric == bestMetric) && (result.m_word != codeword))
        {
            tie = true;
        }
    }

    return found && !tie;
}

bool RadioPagingFlexDecoder::decodeFiw(const RxFrame& frame)
{
    Word word = decodeWord(frame.m_fiw, false);

    if ((word.m_status != WordClean) && (word.m_status != WordCorrected)) {
        return false;
    }

    if (!RadioPagingFlex::checksumValid(word.m_data)) {
        return false;
    }

    m_ctx.m_cycle = (word.m_data >> 4) & 0xf;
    m_ctx.m_frame = (word.m_data >> 8) & 0x7f;
    m_report.m_roaming = ((word.m_data >> 15) & 1) != 0;
    m_report.m_repeat = ((word.m_data >> 16) & 1) != 0;
    m_report.m_traffic = (word.m_data >> 17) & 0xf;
    return true;
}

std::vector<RadioPagingFlexDecoder::Message> RadioPagingFlexDecoder::decodeFrame(const RxFrame& frame)
{
    std::vector<Message> out;

    m_out = &out;
    m_report = FrameReport();
    m_report.m_mode = frame.m_mode;
    m_report.m_inverted = frame.m_inverted;
    m_status.m_frames++;
    m_ctx = FrameContext();
    m_ctx.m_mode = frame.m_mode;
    m_ctx.m_baud = RadioPagingFlex::baud(frame.m_mode);
    m_ctx.m_levels = RadioPagingFlex::levels(frame.m_mode);
    m_ctx.m_bitRate = RadioPagingFlex::bitRate(frame.m_mode);
    m_ctx.m_pol = frame.m_inverted ? 1 : 0;
    m_ctx.m_quality = frame.m_quality;

    bool fiwOk;

    if (frame.m_hasFiw)
    {
        fiwOk = decodeFiw(frame);
    }
    else
    {
        fiwOk = (frame.m_cycle <= 14) && (frame.m_frame <= 127);
        m_ctx.m_cycle = frame.m_cycle;
        m_ctx.m_frame = frame.m_frame;
        m_report.m_roaming = frame.m_roaming;
        m_report.m_repeat = frame.m_repeat;
        m_report.m_traffic = frame.m_traffic & 0xf;
    }

    if (!fiwOk)
    {
        m_status.m_fiwFailures++;
        m_out = nullptr;
        return out;
    }

    m_report.m_fiwValid = true;
    m_report.m_cycle = m_ctx.m_cycle;
    m_report.m_frame = m_ctx.m_frame;

    // Multiple transmission: t1 t0 give the count, t3 t2 the collapse of the repeat interval
    if (m_report.m_repeat)
    {
        unsigned t10 = m_report.m_traffic & 3;
        unsigned t32 = (m_report.m_traffic >> 2) & 3;

        m_report.m_transmissions = (t10 == 1) ? 2 : (t10 == 2) ? 3 : (t10 == 3) ? 4 : 1;
        m_report.m_tdCollapse = (t32 == 1) ? 6 : (t32 == 2) ? 7 : (t32 == 3) ? 5 : -1;
    }

    m_ctx.m_transmissions = m_report.m_transmissions;
    m_ctx.m_tdCollapse = m_report.m_tdCollapse;
    // Frames elapsed since the previous frame of the network: the time components age with them
    const uint32_t elapsed = (absFrame() + framesPerHour - pol().m_lastAbsFrame) % framesPerHour;
    pol().m_lastAbsFrame = absFrame();

    expireTempGroups();
    expireTime(elapsed);

    for (size_t i = 0; (i < frame.m_phases.size()) && (i < (size_t) RadioPagingFlex::m_maxPhases); i++) {
        decodePhase(frame.m_phases[i]);
    }

    // The last frame of its network
    pol().m_report = m_report;
    m_block = nullptr;
    m_out = nullptr;
    return out;
}

// Temporary group assignments that have not been used within 128 frames of the assignment are dropped
// Only the network of the frame: the other polarity is another network with its own frame
// numbering, its state expires with its own frames
std::vector<RadioPagingFlexDecoder::GroupSlot> RadioPagingFlexDecoder::groups(bool inverted) const
{
    std::vector<GroupSlot> out;
    const PolarityState& state = m_pol[inverted ? 1 : 0];

    for (int s = 0; s < m_tempSlots; s++)
    {
        const TempSlot& slot = state.m_temp[s];

        if (slot.m_active)
        {
            GroupSlot g;
            g.m_slot = s;
            g.m_members = slot.m_members;
            g.m_targetFrame = slot.m_targetFrame;
            g.m_setupCycle = slot.m_setupCycle;
            g.m_setupFrame = slot.m_setupFrame;
            out.push_back(g);
        }
    }

    return out;
}

int RadioPagingFlexDecoder::reassemblies(bool inverted) const
{
    int count = 0;

    for (const Reassembly& slot : m_pol[inverted ? 1 : 0].m_reassembly)
    {
        if (slot.m_active) {
            count++;
        }
    }

    return count;
}

int RadioPagingFlexDecoder::historyCount(int polarity) const
{
    int count = 0;

    for (int p = 0; p < 2; p++)
    {
        if ((polarity >= 0) && (p != polarity)) {
            continue;
        }

        for (const HistoryEntry& e : m_pol[p].m_history)
        {
            if (e.m_active) {
                count++;
            }
        }
    }

    return count;
}

void RadioPagingFlexDecoder::expireTempGroups()
{
    uint32_t current = absFrame();

    for (int s = 0; s < m_tempSlots; s++)
    {
        TempSlot& slot = pol().m_temp[s];

        if (!slot.m_active) {
            continue;
        }

        // Pending: from its last assignment; delivering: from its last fragment
        uint32_t last = slot.m_lastFrame;
        uint32_t elapsed = (current >= last) ? (current - last) : (15 * 128 - last + current);

        if (elapsed >= (uint32_t) m_tempTimeout)
        {
            // A group message that started and did not end, or one that never came
            setTempOutcome(s, slot.m_delivering ? GroupTimeout : GroupMissed, slot);
            slot = TempSlot();
        }
    }
}

void RadioPagingFlexDecoder::setTempOutcome(int slot, GroupState state, const TempSlot& group)
{
    TempOutcome& outcome = pol().m_tempOutcome[slot];
    outcome.m_state = state;
    outcome.m_absFrame = absFrame();
    outcome.m_members = (int) group.m_members.size();
    outcome.m_targetFrame = group.m_targetFrame;
}

// The state of each of the 16 slots of the network of the polarity, at its last frame
std::vector<RadioPagingFlexDecoder::GroupSlotState> RadioPagingFlexDecoder::groupStates(bool inverted) const
{
    const PolarityState& state = m_pol[inverted ? 1 : 0];
    std::vector<GroupSlotState> out(m_tempSlots);
    const uint32_t current = state.m_lastAbsFrame;
    auto since = [current](uint32_t frame) {
        return (int) ((current >= frame) ? (current - frame) : (15 * 128 - frame + current));
    };

    for (int s = 0; s < m_tempSlots; s++)
    {
        const TempSlot& slot = state.m_temp[s];
        const TempOutcome& outcome = state.m_tempOutcome[s];
        GroupSlotState& g = out[s];

        if (slot.m_active)
        {
            g.m_members = (int) slot.m_members.size();
            g.m_targetFrame = slot.m_targetFrame;
            g.m_framesAgo = since(slot.m_lastFrame);

            if (slot.m_delivering)
            {
                g.m_state = GroupDelivering;
            }
            else if (g.m_framesAgo == 0)
            {
                g.m_state = GroupAssigning;     // Members assigned in the last frame
            }
            else
            {
                // Waiting for the group message; once its frame went by without it, missed
                int toTarget = ((int) slot.m_targetFrame - (int) (slot.m_lastFrame % 128) + 128) % 128;
                g.m_state = (g.m_framesAgo > toTarget) ? GroupMissed : GroupWaiting;
            }
        }
        else if ((outcome.m_state != GroupFree) && (since(outcome.m_absFrame) < m_outcomeFrames))
        {
            g.m_state = outcome.m_state;
            g.m_members = outcome.m_members;
            g.m_targetFrame = outcome.m_targetFrame;
            g.m_framesAgo = since(outcome.m_absFrame);
        }
    }

    return out;
}

const char *RadioPagingFlexDecoder::groupStateName(GroupState state)
{
    static const char *names[] = {"free", "assigning", "waiting", "delivering", "delivered", "missed", "discarded", "timeout"};
    return names[(int) state];
}

void RadioPagingFlexDecoder::decodePhase(const RxPhase& phase)
{
    m_name = phase.m_name;
    reassemblyExpire();
    historyExpire();

    m_report.m_blocks.push_back(BlockInfo());
    m_block = &m_report.m_blocks.back();
    m_block->m_phase = m_name;

    uint32_t raw[wordsPerFrame];
    bool erased[wordsPerFrame];

    for (int i = 0; i < wordsPerFrame; i++)
    {
        raw[i] = ((size_t) i < phase.m_words.size()) ? phase.m_words[i] : 0;
        erased[i] = ((size_t) i >= phase.m_words.size());
    }

    // The codewords of the other decision on the symbols (their centre)
    uint32_t centre[wordsPerFrame];
    const bool haveCentre = phase.m_centreWords.size() >= (size_t) wordsPerFrame;

    for (int i = 0; i < wordsPerFrame; i++) {
        centre[i] = haveCentre ? phase.m_centreWords[i] : 0;
    }

    for (int b = 0; b < RadioPagingFlex::m_blocksPerFrame; b++)
    {
        RadioPagingFlex::deinterleaveBlock(raw + b * RadioPagingFlex::m_wordsPerBlock);
        RadioPagingFlex::deinterleaveBlock(centre + b * RadioPagingFlex::m_wordsPerBlock);
    }

    for (int i = 0; i < wordsPerFrame; i++)
    {
        if (((size_t) i < phase.m_erased.size()) && phase.m_erased[i]) {
            erased[i] = true;
        }
    }

    // The decision of the phase is the one with fewer codewords that cannot be corrected: the whole symbol in noise,
    // its centre when the band of the signal is narrow for the symbol rate. The other fills in below
    bool centreFirst = false;

    if (haveCentre)
    {
        int lostWhole = 0;
        int lostCentre = 0;

        for (int i = 0; i < wordsPerFrame; i++)
        {
            if (!erased[i])
            {
                lostWhole += (RadioPagingBCH::decode(raw[i]).m_bitErrors < 0) ? 1 : 0;
                lostCentre += (RadioPagingBCH::decode(centre[i]).m_bitErrors < 0) ? 1 : 0;
            }
        }

        if (lostCentre < lostWhole)
        {
            std::swap(raw, centre);
            centreFirst = true;
            m_status.m_phasesFromCentre++;
        }
    }

    // A codeword that cannot be corrected is taken from the other decision if it can be, except in the address field:
    // there a codeword corrected wrong is a message to another pager (the block information word gives the field; not
    // known: all the words after it are taken as addresses)
    unsigned addressStart = 1;
    unsigned vectorStart = wordsPerFrame;

    auto decodeBest = [&](int i) {
        Word word = decodeWord(raw[i], erased[i]);
        const bool addressField = ((unsigned) i >= addressStart) && ((unsigned) i < vectorStart);

        if ((word.m_status == WordUncorrectable) && haveCentre && !erased[i] && !addressField)
        {
            Word other = decodeWord(centre[i], false);

            if ((other.m_status == WordClean) || (other.m_status == WordCorrected))
            {
                raw[i] = centre[i];
                m_status.m_codewordsFromCentre++;
                return other;
            }
        }

        return word;
    };

    Word biw = decodeBest(0);

    if ((biw.m_status == WordClean) || (biw.m_status == WordCorrected))
    {
        addressStart = ((biw.m_data >> 8) & 3) + 1;
        vectorStart = (biw.m_data >> 10) & 0x3f;

        if ((vectorStart <= addressStart) || (vectorStart > (unsigned) wordsPerFrame)) {
            vectorStart = wordsPerFrame;
        }
    }

    for (int i = 0; i < wordsPerFrame; i++)
    {
        m_w[i] = (i == 0) ? biw : decodeBest(i);
        m_status.m_codewords++;

        // A received word that cannot be corrected: its characters are those of the most likely word (the reliabilities
        // are those of the decision on the whole symbol)
        if ((m_w[i].m_status == WordUncorrectable) && m_w[i].m_hasReceived && !centreFirst
            && (phase.m_reliability.size() >= 32 * (size_t) wordsPerFrame))
        {
            // Deinterleaved word i = block * 8 + k: its bit 31 - j is bit j of byte j of the block (interleaved
            // word j / 4, bit 31 - 8 * (j % 4) - k), transmitted as bit index (block * 8 + j / 4) * 32 + 8 * (j % 4) + k
            const int block = i / RadioPagingFlex::m_wordsPerBlock;
            const int k = i % RadioPagingFlex::m_wordsPerBlock;
            uint8_t reliability[32];
            uint32_t codeword;

            for (int j = 0; j < 32; j++) {
                reliability[31 - j] = phase.m_reliability[(block * 8 + j / 4) * 32 + 8 * (j % 4) + k];
            }

            if (softDecode(raw[i], reliability, codeword)) {
                m_w[i].m_received = RadioPagingFlex::wordData(codeword);
            }
        }

        // Erased words were not received: they tell nothing about the bit errors
        if (!erased[i])
        {
            bool uncorrectable = (m_w[i].m_status != WordClean) && (m_w[i].m_status != WordCorrected);
            m_status.m_ber.add(m_w[i].m_bits, 32, uncorrectable);
            pol().m_ber.add(m_w[i].m_bits, 32, uncorrectable);

            if ((m_name >= 'A') && (m_name <= 'D'))
            {
                m_status.m_phaseBer[m_name - 'A'].add(m_w[i].m_bits, 32, uncorrectable);
                pol().m_phaseBer[m_name - 'A'].add(m_w[i].m_bits, 32, uncorrectable);
            }
        }

        switch (m_w[i].m_status)
        {
        case WordClean:
            m_status.m_codewordsClean++;
            break;
        case WordCorrected:
            m_status.m_codewordsCorrected++;
            m_block->m_wordsCorrected++;
            break;
        default:
            m_status.m_codewordsUncorrectable++;
            m_block->m_wordsUncorrectable++;
            if (erased[i]) {
                m_status.m_codewordsErased++;
            }
            break;
        }
    }

    if (m_ctx.m_transmissions > 1)
    {
        int index = std::min(std::max(m_name - 'A', 0), 3);
        decodeSubframes(index);
        return;
    }

    parsePhase();
}

// Picks for each word position the best result of the received copies of a subframe
void RadioPagingFlexDecoder::combineSubframes(SubframeStore& store, Word *out, int& remaining) const
{
    remaining = 0;

    for (int i = 0; i < store.m_words; i++)
    {
        Word best;
        int bestCopy = -1;
        int bestUncorrectable = store.m_words + 1;

        best.m_status = WordUncorrectable;

        for (int c = 0; c < store.m_expected; c++)
        {
            const SubframeCopy& copy = store.m_copies[c];

            if (!copy.m_received) {
                continue;
            }

            const Word& w = copy.m_words[i];

            if (w.m_status == WordClean)
            {
                best = w;
                break;
            }

            if (w.m_status == WordCorrected)
            {
                if ((best.m_status != WordCorrected) || (copy.m_uncorrectable < bestUncorrectable))
                {
                    best = w;
                    bestCopy = c;
                    bestUncorrectable = copy.m_uncorrectable;
                }
            }

            if (bestCopy < 0)
            {
                best.m_data = w.m_data;
                bestCopy = c;
                bestUncorrectable = copy.m_uncorrectable;
            }
        }

        out[i] = best;

        if (best.m_status == WordUncorrectable) {
            remaining++;
        }
    }

    for (int i = store.m_words; i < wordsPerFrame; i++) {
        out[i] = Word();
    }
}

// Multiple transmission (STD-43A 3.4.2, Fig. 3.4.2-3): a frame holds N subframes, subframe k
// (1 to N) carrying the k-th transmission of a content first sent in subframe 1 of the frame
// (k - 1) repeat intervals earlier; the repeat interval is the TD collapse cycle if given
// (3.6), else the system collapse cycle. The subframes of one frame are different contents.
// The copies of a content are collected under the frame of its first transmission and
// combined word by word. The content is decoded once: as soon as its words are all good,
// else with the best words when its last transmission is due. A content whose first
// transmission was missed is decoded from its later copies
void RadioPagingFlexDecoder::decodeSubframes(int phaseIndex)
{
    std::vector<SubframeStore>& stores = pol().m_stores[phaseIndex];

    // The collapse is the network's: kept per polarity
    if ((m_w[0].m_status == WordClean) || (m_w[0].m_status == WordCorrected)) {
        pol().m_lastCollapse = (m_w[0].m_data >> 18) & 7;
    }

    const FrameContext current = m_ctx;
    const char name = m_name;
    int transmissions = std::min(current.m_transmissions, 4);
    int words = subframeWords(transmissions);
    uint32_t interval = 1u << ((current.m_tdCollapse >= 0) ? current.m_tdCollapse : pol().m_lastCollapse);
    uint32_t abs = absFrame();
    Word frame[wordsPerFrame];

    std::copy(m_w, m_w + wordsPerFrame, frame);

    for (int slot = 0; slot < transmissions; slot++)
    {
        const Word *w = &frame[slot * words];

        // A subframe without addresses is idle
        if (w[0].m_status != WordUncorrectable)
        {
            unsigned vector = (w[0].m_data >> 10) & 0x3f;
            unsigned address = ((w[0].m_data >> 8) & 3) + 1;

            if (vector == address) {
                continue;
            }
        }

        uint32_t origin = (abs + framesPerHour - ((uint32_t) slot * interval) % framesPerHour) % framesPerHour;
        SubframeStore *store = nullptr;

        for (SubframeStore& s : stores)
        {
            if ((s.m_origin == origin) && (s.m_expected == transmissions) && (s.m_interval == interval))
            {
                store = &s;
                break;
            }
        }

        if (!store)
        {
            stores.push_back(SubframeStore());
            store = &stores.back();
            store->m_origin = origin;
            store->m_interval = interval;
            store->m_expected = transmissions;
            store->m_words = words;
            store->m_phase = name;
            store->m_context = current;
            store->m_context.m_cycle = origin / 128;
            store->m_context.m_frame = origin % 128;
        }

        SubframeCopy& copy = store->m_copies[slot];

        if (copy.m_received) {
            continue;
        }

        copy.m_received = true;
        copy.m_uncorrectable = 0;

        for (int i = 0; i < words; i++)
        {
            copy.m_words[i] = w[i];

            if (w[i].m_status == WordUncorrectable) {
                copy.m_uncorrectable++;
            }
        }

        store->m_received++;

        if (!store->m_decoded)
        {
            Word combined[wordsPerFrame];
            int remaining;

            combineSubframes(*store, combined, remaining);

            if (remaining == 0) {
                decodeStore(*store);
            }
        }
    }

    // Contents whose last transmission is due: decoded with what was received, then dropped
    bool waiting = false;

    for (auto it = stores.begin(); it != stores.end();)
    {
        uint32_t age = (abs + framesPerHour - it->m_origin) % framesPerHour;

        if (age >= (uint32_t) (it->m_expected - 1) * it->m_interval)
        {
            if (!it->m_decoded) {
                decodeStore(*it);
            }

            it = stores.erase(it);
        }
        else
        {
            waiting = waiting || !it->m_decoded;
            ++it;
        }
    }

    m_ctx = current;
    m_name = name;

    if (waiting) {
        m_block->m_waiting = true;
    }
}

void RadioPagingFlexDecoder::decodeStore(SubframeStore& store)
{
    Word combined[wordsPerFrame];
    int remaining;

    combineSubframes(store, combined, remaining);
    store.m_decoded = true;
    m_ctx = store.m_context;
    m_ctx.m_copies = store.m_received;
    m_name = store.m_phase;
    std::copy(combined, combined + wordsPerFrame, m_w);
    parsePhase();
}

std::vector<RadioPagingFlexDecoder::Message> RadioPagingFlexDecoder::flush()
{
    std::vector<Message> out;

    m_out = &out;
    m_report = FrameReport();

    // Contents of a multiple transmission still waiting for copies: decoded with the copies received
    for (int p = 0; p < 2; p++)
    {
        for (int i = 0; i < 4; i++)
        {
            for (SubframeStore& store : m_pol[p].m_stores[i])
            {
                if (!store.m_decoded && (store.m_received > 0))
                {
                    m_report.m_blocks.push_back(BlockInfo());
                    m_block = &m_report.m_blocks.back();
                    m_block->m_phase = store.m_phase;
                    decodeStore(store);
                }
            }

            m_pol[p].m_stores[i].clear();
        }
    }

    for (int p = 0; p < 2; p++)
    {
        for (Reassembly& slot : m_pol[p].m_reassembly)
        {
            if (slot.m_active)
            {
                emitIncomplete(slot);
                slot.m_active = false;
            }
        }
    }

    m_block = nullptr;
    m_out = nullptr;
    return out;
}

// Network time

int RadioPagingFlexDecoder::zoneMinutes(unsigned zone)
{
    int minutes = 0;
    RadioPagingFlex::timeZoneMinutes(zone, minutes);
    return minutes;
}

bool RadioPagingFlexDecoder::dateValid(int year, unsigned month, unsigned day)
{
    static const unsigned days[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if ((month < 1) || (month > 12) || (day < 1)) {
        return false;
    }

    unsigned maxDay = days[month];

    if (month == 2)
    {
        unsigned y = (unsigned) year;

        if (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0)) {
            maxDay = 29;
        }
    }

    return day <= maxDay;
}

void RadioPagingFlexDecoder::nextDay(int y, unsigned m, unsigned d, int& ny, unsigned& nm, unsigned& nd)
{
    static const unsigned days[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned maxDay = days[m];

    if (m == 2)
    {
        unsigned yu = (unsigned) y;

        if (((yu % 4 == 0) && (yu % 100 != 0)) || (yu % 400 == 0)) {
            maxDay = 29;
        }
    }

    if (d < maxDay) {
        ny = y; nm = m; nd = d + 1;
    } else if (m < 12) {
        ny = y; nm = m + 1; nd = 1;
    } else {
        ny = y + 1; nm = 1; nd = 1;
    }
}

// Same or adjacent calendar days
bool RadioPagingFlexDecoder::datesAgree(int y1, unsigned m1, unsigned d1, int y2, unsigned m2, unsigned d2)
{
    int ny;
    unsigned nm;
    unsigned nd;

    if ((y1 == y2) && (m1 == m2) && (d1 == d2)) {
        return true;
    }

    nextDay(y1, m1, d1, ny, nm, nd);

    if ((ny == y2) && (nm == m2) && (nd == d2)) {
        return true;
    }

    nextDay(y2, m2, d2, ny, nm, nd);
    return (ny == y1) && (nm == m1) && (nd == d1);
}

// The minute of the time word must lie in the 4 minutes of the cycle of the frame. A cycle starts every 4
// minutes from the hour of the frame grid; the time word gives the local time, so the minute is shifted by the
// part of the zone offset that is not whole hours (30 or 45 minutes in Table 3.7.2-3) when the grid follows
// the hours of UTC, and not shifted when it follows the local hours. With the zone not known every shift of
// the table is accepted
bool RadioPagingFlexDecoder::timeMatchesCycle(unsigned minute, unsigned cycle, int zoneOffset)
{
    int shifts[3] = {0, 30, 45};
    int count = 3;

    if (zoneOffset != INT_MIN)
    {
        shifts[1] = ((zoneOffset % 60) + 60) % 60;
        count = (shifts[1] == 0) ? 1 : 2;
    }

    for (int i = 0; i < count; i++)
    {
        int diff = (((int) minute - (int) (cycle * 4) - shifts[i]) % 60 + 60) % 60;

        if (diff <= 3) {
            return true;
        }
    }

    return false;
}

void RadioPagingFlexDecoder::voteDate(int year, unsigned month, unsigned day)
{
    BiwState& b = pol().m_biw;

    b.m_dateYear[b.m_dateRingIndex] = year;
    b.m_dateMonth[b.m_dateRingIndex] = month;
    b.m_dateDay[b.m_dateRingIndex] = day;
    b.m_dateRingIndex = (b.m_dateRingIndex + 1) % m_voteRing;

    if (b.m_dateRingCount < m_voteRing) {
        b.m_dateRingCount++;
    }

    int agree = 0;

    for (int i = 0; i < b.m_dateRingCount; i++)
    {
        if (datesAgree(year, month, day, b.m_dateYear[i], b.m_dateMonth[i], b.m_dateDay[i])) {
            agree++;
        }
    }

    if (agree >= m_voteThreshold)
    {
        b.m_year = year;
        b.m_month = month;
        b.m_day = day;
        b.m_seenDate = true;
        b.m_dateAge = 0;
    }
}

void RadioPagingFlexDecoder::voteTime(unsigned hour, unsigned minute, unsigned second)
{
    BiwState& b = pol().m_biw;

    if (b.m_voteTimeCount > 0)
    {
        int previous = (int) (b.m_voteHour * 60 + b.m_voteMinute);
        int current = (int) (hour * 60 + minute);
        int delta = current - previous;

        if (delta < -720) {
            delta += 1440;
        }
        if (delta < 0) {
            b.m_voteTimeCount = 0;
        }
    }

    b.m_voteHour = hour;
    b.m_voteMinute = minute;
    b.m_voteTimeCount++;

    if (b.m_voteTimeCount >= m_voteThreshold)
    {
        b.m_hour = hour;
        b.m_minute = minute;
        b.m_second = second;
        b.m_seenTime = true;
        b.m_timeAge = 0;
        b.m_timeFrame = m_ctx.m_frame;
    }

    // Mode detection. Time of frame 0: the same for the whole cycle. Time of the frame: changes
    // every 4 frames. Observations at least 5 frames apart in a cycle tell the two apart
    if (b.m_modeValid && (b.m_modeCycle == m_ctx.m_cycle))
    {
        int distance = std::abs((int) m_ctx.m_frame - (int) b.m_modeFrame);

        if (distance >= 5)
        {
            if ((minute == b.m_modeMinute) && (second == b.m_modeSecond))
            {
                b.m_modeVotesStd++;
                b.m_modeVotesDir = 0;
            }
            else
            {
                b.m_modeVotesDir++;
                b.m_modeVotesStd = 0;
            }

            if (b.m_modeVotesStd >= m_voteThreshold)
            {
                b.m_modeDetected = 0;
                b.m_modeConfirmed = true;
            }
            if (b.m_modeVotesDir >= m_voteThreshold)
            {
                b.m_modeDetected = 1;
                b.m_modeConfirmed = true;
            }

            b.m_modeMinute = minute;
            b.m_modeSecond = second;
            b.m_modeFrame = m_ctx.m_frame;
        }
    }
    else
    {
        b.m_modeMinute = minute;
        b.m_modeSecond = second;
        b.m_modeFrame = m_ctx.m_frame;
        b.m_modeCycle = m_ctx.m_cycle;
        b.m_modeValid = true;
    }
}

void RadioPagingFlexDecoder::voteZone(unsigned zone, unsigned dst, unsigned ext)
{
    BiwState& b = pol().m_biw;

    b.m_zoneZone[b.m_zoneRingIndex] = zone;
    b.m_zoneDst[b.m_zoneRingIndex] = dst;
    b.m_zoneExt[b.m_zoneRingIndex] = ext;
    b.m_zoneRingIndex = (b.m_zoneRingIndex + 1) % m_voteRing;

    if (b.m_zoneRingCount < m_voteRing) {
        b.m_zoneRingCount++;
    }

    int agree = 0;

    for (int i = 0; i < b.m_zoneRingCount; i++)
    {
        if ((b.m_zoneZone[i] == zone) && (b.m_zoneDst[i] == dst) && (b.m_zoneExt[i] == ext)) {
            agree++;
        }
    }

    if (agree >= m_voteThreshold)
    {
        b.m_zone = zone;
        b.m_zoneDefined = RadioPagingFlex::timeZoneMinutes(zone, b.m_zoneMinutes);
        b.m_dst = dst;
        b.m_extSec = ext;
        b.m_seenZone = true;
        b.m_zoneAge = 0;
    }
}

// The time components age with the frames of the network: elapsed is the number of frames since its previous frame
// (from the frame numbers, a gap of up to an hour). One not confirmed again for a little over an hour (a network sends
// the time at least once an hour, STD-43A 3.7.2) is forgotten, and its votes start again
void RadioPagingFlexDecoder::expireTime(uint32_t elapsed)
{
    BiwState& b = pol().m_biw;
    const uint32_t expire = framesPerHour + 5;

    b.m_dateAge += elapsed;
    b.m_timeAge += elapsed;
    b.m_zoneAge += elapsed;

    if (b.m_seenDate && (b.m_dateAge >= expire))
    {
        b.m_seenDate = false;
        b.m_dateRingCount = 0;
        b.m_dateRingIndex = 0;
    }
    if (b.m_seenTime && (b.m_timeAge >= expire))
    {
        b.m_seenTime = false;
        b.m_voteTimeCount = 0;
    }
    if (b.m_seenZone && (b.m_zoneAge >= expire))
    {
        b.m_seenZone = false;
        b.m_zoneRingCount = 0;
        b.m_zoneRingIndex = 0;
    }
}

RadioPagingFlexDecoder::NetworkTime RadioPagingFlexDecoder::networkTime(bool inverted) const
{
    const BiwState& b = m_pol[inverted ? 1 : 0].m_biw;
    NetworkTime t;

    t.m_dateValid = b.m_seenDate;
    t.m_year = b.m_year;
    t.m_month = b.m_month;
    t.m_day = b.m_day;
    t.m_timeValid = b.m_seenTime;
    t.m_hour = b.m_hour;
    t.m_minute = b.m_minute;
    t.m_second7_5 = b.m_second;
    t.m_zoneValid = b.m_seenZone;
    t.m_zone = b.m_zone;
    t.m_zoneDefined = b.m_zoneDefined;
    t.m_zoneMinutes = b.m_zoneMinutes;
    t.m_dst = b.m_dst;
    t.m_extendedSeconds = b.m_extSec;
    t.m_mode = b.m_modeDetected;
    t.m_modeConfirmed = b.m_modeConfirmed;

    // Frames since each was confirmed, as of the last frame of the network
    t.m_dateAge = b.m_seenDate ? (int) b.m_dateAge : -1;
    t.m_timeAge = b.m_seenTime ? (int) b.m_timeAge : -1;
    t.m_zoneAge = b.m_seenZone ? (int) b.m_zoneAge : -1;
    t.m_ssid1Valid = b.m_ssid1;
    t.m_localId = b.m_localId;
    t.m_coverageZone = b.m_coverage;
    t.m_ssid2Valid = b.m_ssid2;
    t.m_country = b.m_country;
    t.m_tmf = b.m_tmf;

    // The time of the last frame of the network: the time word, plus the frames since the frame it is the time of.
    // Standard: it is the time of frame 0 of the cycle of the frame that carried it; direct: of that frame. Then the
    // frames since that frame (1.875 s each), so that the time is right for the whole hour a network may go without
    // sending it, not only in the cycle it came in
    if (b.m_seenTime)
    {
        double seconds = b.m_hour * 3600.0 + b.m_minute * 60.0 + b.m_second * 7.5;

        if (b.m_seenZone) {
            seconds += b.m_extSec * 0.9375;
        }
        if (b.m_modeDetected == 0) {
            seconds += b.m_timeFrame * 1.875;
        }

        seconds = std::fmod(seconds + b.m_timeAge * 1.875, 86400.0);
        const int whole = (int) seconds;
        t.m_currentHour = (unsigned) (whole / 3600);
        t.m_currentMinute = (unsigned) ((whole % 3600) / 60);
        t.m_currentSecond = seconds - (whole - whole % 60);
    }

    return t;
}

// Extra block information words

void RadioPagingFlexDecoder::parseExtraBiw(int index)
{
    ExtraBiw e;
    BiwState& b = pol().m_biw;

    e.m_index = index;

    if ((m_w[index].m_status == WordUncorrectable) || (m_w[index].m_status == WordNotReceived))
    {
        e.m_kind = ExtraBiw::Lost;
        m_block->m_extra.push_back(e);
        return;
    }

    uint32_t word = m_w[index].m_data;
    unsigned type = (word >> 4) & 7;

    e.m_word = word;

    switch (type)
    {
    case 0:
        e.m_kind = ExtraBiw::Ssid1;
        e.m_coverageZone = (word >> 7) & 0x1f;
        e.m_localId = (word >> 12) & 0x1ff;
        b.m_localId = e.m_localId;
        b.m_coverage = e.m_coverageZone;
        b.m_ssid1 = true;
        break;
    case 1:
    {
        e.m_kind = ExtraBiw::Date;
        unsigned yearField = (word >> 7) & 0x1f;
        e.m_day = (word >> 12) & 0x1f;
        e.m_month = (word >> 17) & 0xf;

        int real;

        if (m_fixedYear)
        {
            real = 1994 + (int) yearField;
        }
        else
        {
            // The year field is the year in a period of 32 years starting at 1994
            int era = (m_referenceYear - 1994) / 32;
            real = 1994 + era * 32 + (int) yearField;

            if (real - m_referenceYear > 16) {
                real -= 32;
            } else if (m_referenceYear - real > 16) {
                real += 32;
            }
        }

        e.m_year = real;
        e.m_dateValid = dateValid(real, e.m_month, e.m_day);

        if (e.m_dateValid) {
            voteDate(real, e.m_month, e.m_day);
        }
        break;
    }
    case 2:
    {
        e.m_kind = ExtraBiw::Time;
        e.m_hour = (word >> 7) & 0x1f;
        e.m_minute = (word >> 12) & 0x3f;
        e.m_second7_5 = (word >> 18) & 7;

        if ((e.m_hour > 23) || (e.m_minute > 59))
        {
            // Out of range
        }
        else if (!timeMatchesCycle(e.m_minute, m_ctx.m_cycle, (b.m_seenZone && b.m_zoneDefined) ? b.m_zoneMinutes : INT_MIN))
        {
            b.m_voteTimeCount = 0;
        }
        else
        {
            e.m_timeValid = true;
            voteTime(e.m_hour, e.m_minute, e.m_second7_5);
        }
        break;
    }
    case 5:
    {
        e.m_kind = ExtraBiw::SystemInfo;
        e.m_infoType = (word >> 7) & 0xf;
        e.m_info = (word >> 11) & 0x3ff;

        if ((e.m_infoType == 4) || (e.m_infoType == 5))
        {
            e.m_zone = e.m_info & 0x1f;
            e.m_dst = (e.m_info >> 5) & 1;
            e.m_extendedSeconds = (e.m_info >> 7) & 7;
            e.m_zoneDefined = RadioPagingFlex::timeZoneMinutes(e.m_zone, e.m_zoneMinutes);
            voteZone(e.m_zone, e.m_dst, e.m_extendedSeconds);
        }
        else if (e.m_infoType <= 3)
        {
            // System message: its vector is at the end of the vector field
            b.m_sysMsgType = (int) e.m_infoType;
        }
        else if (e.m_infoType == 6)
        {
            e.m_frameOffset = e.m_info & 0x3f;
            e.m_maxCarryOn = (e.m_info >> 6) & 3;
            e.m_nidMessage = ((e.m_info >> 8) & 1) != 0;
            e.m_systemMessage = ((e.m_info >> 9) & 1) != 0;
        }
        break;
    }
    case 7:
        e.m_kind = ExtraBiw::Ssid2;
        e.m_tmf = (word >> 7) & 0xf;
        e.m_country = (word >> 11) & 0x3ff;
        b.m_country = e.m_country;
        b.m_tmf = e.m_tmf;
        b.m_ssid2 = true;
        break;
    default:
        e.m_kind = ExtraBiw::Reserved;
        break;
    }

    m_block->m_extra.push_back(e);
}

void RadioPagingFlexDecoder::parsePhase()
{
    if ((m_w[0].m_status == WordUncorrectable) || (m_w[0].m_status == WordNotReceived)) {
        return;
    }

    uint32_t biw = m_w[0].m_data;
    unsigned priority = (biw >> 4) & 0xf;
    unsigned vectorStart = (biw >> 10) & 0x3f;
    unsigned addressStart = ((biw >> 8) & 3) + 1;

    m_block->m_priorityWords = priority;
    m_block->m_addressStart = addressStart;
    m_block->m_vectorStart = vectorStart;
    m_block->m_carryOn = (biw >> 16) & 3;
    m_block->m_collapse = (biw >> 18) & 7;

    if ((vectorStart < addressStart) || (vectorStart >= (unsigned) wordsPerFrame)) {
        return;
    }

    m_block->m_valid = true;
    pol().m_biw.m_sysMsgType = -1;

    // Words 1 to addressStart - 1 are extra block information words, identified by their type field.
    // They are decoded before the idle check: an idle phase can carry date, time and identification
    for (unsigned i = 1; (i < addressStart) && (i < (unsigned) wordsPerFrame); i++) {
        parseExtraBiw((int) i);
    }

    if (vectorStart == addressStart)
    {
        m_block->m_idle = true;
        return;
    }

    walkAddresses(addressStart, vectorStart, priority);
}

// Messages

bool RadioPagingFlexDecoder::wordOk(int index) const
{
    return (index >= 0) && (index < wordsPerFrame)
        && ((m_w[index].m_status == WordClean) || (m_w[index].m_status == WordCorrected));
}

void RadioPagingFlexDecoder::countWord(Message& msg, const Word& word) const
{
    if (word.m_status == WordCorrected)
    {
        msg.m_wordsCorrected++;
        msg.m_bitErrors += word.m_bits;

        if (word.m_parity) {
            msg.m_parityCorrections++;
        }
    }
    else if ((word.m_status == WordUncorrectable) || (word.m_status == WordNotReceived))
    {
        msg.m_wordsUncorrectable++;
    }
}

void RadioPagingFlexDecoder::countWords(Message& msg, int first, int last) const
{
    for (int i = std::max(first, 0); (i <= last) && (i < wordsPerFrame); i++) {
        countWord(msg, m_w[i]);
    }
}

void RadioPagingFlexDecoder::setRaw(Message& msg, const std::vector<uint32_t>& words)
{
    msg.m_rawData.assign((words.size() * 21 + 7) / 8, 0);
    msg.m_rawBits = (int) words.size() * 21;

    size_t bit = 0;

    for (uint32_t word : words)
    {
        for (int b = 0; b < 21; b++, bit++)
        {
            if ((word >> b) & 1) {
                msg.m_rawData[bit / 8] |= 0x80 >> (bit % 8);
            }
        }
    }
}

RadioPagingFlexDecoder::Message RadioPagingFlexDecoder::newMessage(const AddressContext& a) const
{
    Message msg;

    msg.m_capcode = a.m_capcode;
    msg.m_address = RadioPagingFlex::addressText(a.m_capcode);
    msg.m_longAddress = a.m_long;
    msg.m_addressType = a.m_type;
    msg.m_priority = a.m_priority;
    msg.m_mode = m_ctx.m_mode;
    msg.m_baud = m_ctx.m_baud;
    msg.m_levels = m_ctx.m_levels;
    msg.m_bitRate = m_ctx.m_bitRate;
    msg.m_inverted = (m_ctx.m_pol == 1);
    msg.m_quality = m_ctx.m_quality;
    msg.m_cycle = m_ctx.m_cycle;
    msg.m_frame = m_ctx.m_frame;
    msg.m_firstCycle = m_ctx.m_cycle;
    msg.m_firstFrame = m_ctx.m_frame;
    msg.m_phase = m_name;
    msg.m_transmissions = m_ctx.m_transmissions;
    msg.m_copies = (m_ctx.m_transmissions > 1) ? m_ctx.m_copies : 1;
    msg.m_tempSlot = a.m_tempSlot;
    return msg;
}

void RadioPagingFlexDecoder::emitMessage(Message& msg)
{
    msg.m_typeName = messageTypeName(msg.m_type);

    if (msg.m_address.empty()) {
        msg.m_address = RadioPagingFlex::addressText(msg.m_capcode);
    }

    m_status.m_messages++;

    if (!msg.m_complete) {
        m_status.m_incomplete++;
    }

    // A message of fragments one of which is missing: the checksum K of the missing fragment is not ok, and a
    // reassembled message has the worst K of its fragments
    if (msg.m_fragmented && !msg.m_isFragment && !msg.m_complete) {
        msg.m_checksum = CheckFailed;
    }

    if (m_out) {
        m_out->push_back(msg);
    }
}

// Fragment reassembly. A stream is identified by capcode and message number

int RadioPagingFlexDecoder::reassemblyFind(uint64_t capcode, int number)
{
    std::vector<Reassembly>& slots = pol().m_reassembly;

    for (int i = 0; i < m_reassemblySlots; i++)
    {
        if (slots[i].m_active && (slots[i].m_capcode == capcode) && (slots[i].m_number == number)) {
            return i;
        }
    }

    return -1;
}

int RadioPagingFlexDecoder::reassemblyAlloc(uint64_t capcode, int number, int vector)
{
    std::vector<Reassembly>& slots = pol().m_reassembly;
    int index = -1;

    for (int i = 0; i < m_reassemblySlots; i++)
    {
        if (!slots[i].m_active)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        // Evict the slot that has waited longest
        index = 0;

        for (int i = 1; i < m_reassemblySlots; i++)
        {
            if (slots[i].m_lastFrame < slots[index].m_lastFrame) {
                index = i;
            }
        }

        emitIncomplete(slots[index]);
    }

    slots[index] = Reassembly();
    slots[index].m_active = true;
    slots[index].m_capcode = capcode;
    slots[index].m_number = number;
    slots[index].m_vector = vector;
    slots[index].m_lastFrame = m_ctx.m_frame;
    slots[index].m_lastCycle = m_ctx.m_cycle;
    return index;
}

// A first fragment of an address that has a fragment stream open abandons the stream: an address sends one
// fragmented message at a time (STD-43A 3.8.5 (2), 4.2 (1)). An unfragmented message does not: the address can
// have one in a frame while it sends a fragmented message (3.8.5 (3), 4.2.3)
void RadioPagingFlexDecoder::reassemblyInvalidate(uint64_t capcode, int excludeNumber)
{
    for (Reassembly& slot : pol().m_reassembly)
    {
        if (!slot.m_active || (slot.m_capcode != capcode)) {
            continue;
        }

        if ((excludeNumber >= 0) && (slot.m_number == excludeNumber)) {
            continue;
        }

        emitIncomplete(slot);
        slot.m_active = false;
    }
}

// Fragments more than 64 frames apart: the stream is released as incomplete message. Only the
// network of the frame, by its own frame numbering
void RadioPagingFlexDecoder::reassemblyExpire()
{
    uint32_t current = absFrame();

    for (Reassembly& slot : pol().m_reassembly)
    {
        if (!slot.m_active) {
            continue;
        }

        int delta = (int) current - (int) (slot.m_lastCycle * 128 + slot.m_lastFrame);

        if (delta < 0) {
            delta += 15 * 128;
        }

        if (delta > m_reassemblyTimeout)
        {
            emitIncomplete(slot);
            slot.m_active = false;
        }
    }
}

void RadioPagingFlexDecoder::appendGap(Reassembly& slot)
{
    slot.m_text += m_gapCode;
    slot.m_charStatus += 'G';
}

void RadioPagingFlexDecoder::emitIncomplete(Reassembly& slot)
{
    if (slot.m_fragments == 0) {
        return;
    }

    Message msg = slot.m_message;

    // The fragments that did not come: a lost fragment of unknown length at the end (characters only: secure hex
    // marks its lost fragments in its bits)
    if (!slot.m_charStatus.empty()) {
        appendGap(slot);
    }

    msg.m_text = slot.m_text;
    msg.m_charStatus = slot.m_charStatus;
    msg.m_fragmented = true;
    msg.m_fragmentCount = slot.m_fragments;
    msg.m_complete = false;
    msg.m_fragmentGap = slot.m_gap;
    msg.m_isFragment = false;
    setRaw(msg, slot.m_raw);

    if (slot.m_signatureValid == false) {
        msg.m_signature = CheckIncomplete;
    }

    emitMessage(msg);
}

// Message number sequence of an address (STD-43A 3.10.1.3 N and R, 4.3.7): the numbers of the messages with R = 1
// ascend by one up to the largest number of the network, then 0 again. A number further on tells messages were
// missed; the same number again is a copy of the last message. Messages with R = 0 are out of the sequence
void RadioPagingFlexDecoder::checkSequence(Message& msg, const AddressContext& a)
{
    if (!msg.m_retrieval || (msg.m_messageNumber < 0) || (a.m_type == AddressTemporary)) {
        return;
    }

    const uint64_t capcode = a.m_capcode;
    std::unordered_map<uint64_t, int>& last = pol().m_lastNumbers;
    auto found = last.find(capcode);
    const int number = msg.m_messageNumber & 0x3f;

    if (found != last.end())
    {
        // A number above the largest of the setting: the network counts further, 63 at most
        const int modulus = std::max(m_maxMessageNumber, std::max(number, found->second)) + 1;
        int expected = (found->second + 1) % modulus;

        if (number != found->second)
        {
            for (int n = expected; n != number; n = (n + 1) % modulus) {
                msg.m_missedNumbers.push_back(n);
            }
        }

        found->second = number;
        return;
    }

    if (last.size() >= m_maxSequenceAddresses) {
        last.clear();
    }

    last[capcode] = number;
}

// Message history: as a pager, the last messages of each capcode, for duplicates and the repair of a copy with lost words

RadioPagingFlexDecoder::HistoryEntry *RadioPagingFlexDecoder::historyFind(uint64_t capcode, int number, int vector, int phase,
    const Word *words, int wordCount, int fragCount, bool isLong, int blocking)
{
    for (HistoryEntry& e : pol().m_history)
    {
        if (!e.m_active || (e.m_capcode != capcode) || (e.m_number != number) || (e.m_vector != vector) || (e.m_phase != phase)
            || (e.m_wordCount != wordCount) || (e.m_fragCount != fragCount) || (e.m_long != isLong) || (e.m_blocking != blocking))
        {
            continue;
        }

        bool same = true;

        for (int i = 0; same && (i < wordCount) && (i < wordsPerFrame); i++)
        {
            bool storedOk = (e.m_words[i].m_status == WordClean) || (e.m_words[i].m_status == WordCorrected);
            bool wordOk = (words[i].m_status == WordClean) || (words[i].m_status == WordCorrected);
            uint32_t mask = retransmissionMask(vector, isLong, i);

            same = !(storedOk && wordOk && ((e.m_words[i].m_data & mask) != (words[i].m_data & mask)));
        }

        if (same) {
            return &e;
        }
    }

    return nullptr;
}

RadioPagingFlexDecoder::HistoryEntry *RadioPagingFlexDecoder::historyStore(uint64_t capcode, int number, int vector, int phase,
    const Word *words, int wordCount, int fragCount, bool isLong, int blocking, uint32_t frame)
{
    std::vector<HistoryEntry>& history = pol().m_history;
    HistoryEntry *target = nullptr;
    int own = 0;
    uint32_t ownOldestAge = 0;
    HistoryEntry *ownOldest = nullptr;

    for (HistoryEntry& e : history)
    {
        if (e.m_active && (e.m_capcode == capcode))
        {
            uint32_t age = (frame - e.m_frame + framesPerHour) % framesPerHour;

            own++;
            if (!ownOldest || (age > ownOldestAge))
            {
                ownOldestAge = age;
                ownOldest = &e;
            }
        }
    }

    if (own >= m_historyBacklog) {
        target = ownOldest;
    }

    if (!target)
    {
        for (HistoryEntry& e : history)
        {
            if (!e.m_active)
            {
                target = &e;
                break;
            }
        }
    }

    if (!target)
    {
        // Full: replace the oldest
        uint32_t oldestAge = 0;
        size_t oldest = 0;

        for (size_t i = 0; i < history.size(); i++)
        {
            uint32_t age = (frame - history[i].m_frame + framesPerHour) % framesPerHour;

            if (age > oldestAge)
            {
                oldestAge = age;
                oldest = i;
            }
        }

        target = &history[oldest];
    }

    *target = HistoryEntry();
    target->m_active = true;
    target->m_capcode = capcode;
    target->m_number = number;
    target->m_vector = vector;
    target->m_phase = phase;
    target->m_wordCount = wordCount;
    target->m_fragCount = fragCount;
    target->m_long = isLong;
    target->m_blocking = blocking;
    target->m_frame = frame;

    int count = std::min(std::max(wordCount, 0), wordsPerFrame);

    for (int i = 0; i < count; i++)
    {
        target->m_words[i] = words[i];

        if (words[i].m_status == WordUncorrectable) {
            target->m_hasUncorrectable = true;
        }
    }

    return target;
}

// Only the network of the frame, by its own frame numbering
void RadioPagingFlexDecoder::historyExpire()
{
    uint32_t current = absFrame();

    for (HistoryEntry& e : pol().m_history)
    {
        if (!e.m_active) {
            continue;
        }

        uint32_t age = (current - e.m_frame + framesPerHour) % framesPerHour;

        if (age >= (uint32_t) m_historyWindow) {
            e.m_active = false;
        }
    }
}

// Bits of a message word that are the same in a retransmission of the message: all but the checksum K and the
// retrieval flag R, which is 0 in a retransmission (STD-43A 3.10.1.1-3.10.1.3; 4.3.7.6). words start at the first
// message word of the message field: the first header word of a long address is in the vector field
uint32_t RadioPagingFlexDecoder::retransmissionMask(int vector, bool isLong, int index)
{
    switch (vector)
    {
    case 5:     // Alphanumeric: header K bits 0-9, R bit 19
        return (!isLong && (index == 0)) ? ~(0x3ffu | (1u << 19)) : 0x1fffffu;
    case 6:     // HEX/binary: header word 1 K bits 0-11, header word 2 R bit 0
        if (!isLong) {
            return (index == 0) ? ~0xfffu : (index == 1) ? ~1u : 0x1fffffu;
        }
        return (index == 0) ? ~1u : 0x1fffffu;
    case 7:     // Numbered numeric: first word K bits 0-1, R bit 8
        return (!isLong && (index == 0)) ? ~(0x3u | (1u << 8)) : 0x1fffffu;
    default:
        return 0x1fffffu;
    }
}

// Returns 0: a new message, 1: a duplicate (a copy without gain), -1: a duplicate whose words repair the stored copy
// (m_historyMatch): uncorrectable words of this copy are replaced. A duplicate is output again, marked as such. As a
// pager the decoder keeps the last messages of each capcode and takes a message that is one of them again as a
// duplicate, whatever its R: a network without message numbering sends every message with N = 0 and R = 0 (8.4), and
// a numbered message sent again with a new number is a new message
int RadioPagingFlexDecoder::checkRetransmission(uint64_t capcode, bool retrieval, int number, int vector, int phase,
    const Word *words, int wordCount, int fragCount, bool isLong, int blocking)
{
    (void) retrieval;
    uint32_t frame = absFrame();
    HistoryEntry *entry = historyFind(capcode, number, vector, phase, words, wordCount, fragCount, isLong, blocking);

    m_historyMatch = entry;

    if (!entry)
    {
        historyStore(capcode, number, vector, phase, words, wordCount, fragCount, isLong, blocking, frame);
        return 0;
    }

    if (!entry->m_hasUncorrectable)
    {
        m_status.m_duplicates++;
        return 1;
    }

    // The earlier copy had lost words: take the ones this copy has
    int recovered = 0;

    for (int i = 0; (i < wordCount) && (i < entry->m_wordCount) && (i < wordsPerFrame); i++)
    {
        if ((entry->m_words[i].m_status == WordUncorrectable)
            && ((words[i].m_status == WordClean) || (words[i].m_status == WordCorrected)))
        {
            entry->m_words[i] = words[i];
            recovered++;
        }
    }

    if (recovered > 0)
    {
        entry->m_hasUncorrectable = false;

        for (int i = 0; (i < entry->m_wordCount) && (i < wordsPerFrame); i++)
        {
            if (entry->m_words[i].m_status == WordUncorrectable)
            {
                entry->m_hasUncorrectable = true;
                break;
            }
        }

        entry->m_frame = frame;
        m_status.m_recovered++;
        return -1;
    }

    m_status.m_duplicates++;
    return 1;
}
