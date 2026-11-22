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

#include "radiopagingflexreceiver.h"

#include <algorithm>
#include <cmath>

#include "radiopagingbch.h"

const int RadioPagingFlexReceiver::m_maxSyncErrors;
constexpr float RadioPagingFlexReceiver::m_erasureDb;

namespace {

// Mode codes A (32 bits) and inverted A, in transmission order, for the four modes
const uint32_t codeA[4] = {0x78f35939u, 0x84e75939u, 0x4f975939u, 0x215f5939u};
const uint32_t codeAInv[4] = {0x870ca6c6u, 0x7b18a6c6u, 0xb068a6c6u, 0xdea0a6c6u};
const uint32_t codeB = 0x5555u;
// Frame period (STD-43A 3.1.1: frames 0-127 of cycles 0-14, one after the other)
const double frameSeconds = 1.875;
// Closing pattern of the second sync
const uint32_t syncCInv = 0x127bu;
// Gray decode of 4-level symbols to dibits: 0 -> 00, 1 -> 01, 2 -> 11, 3 -> 10
const unsigned grayToDibit[4] = {0, 1, 3, 2};
// Sync 1 (A, B, inverted A) lasts 80 bits at 1600 baud
const int syncBits = 80;
// Half a cycle of the emergency re-synchronization, BS and Ar (STD-43A Table 3.2.1-1); the
// other half is its inverse
const uint64_t ersHalf = 0xaaaacb205939ULL;
const uint64_t mask48 = 0xffffffffffffULL;
const int ersCycleBits = 96;
// Cycles missed in noise do not split a re-synchronization in two: a new one after 3 cycles
const int ersGapBits = 3 * ersCycleBits;

int popcount64(uint64_t x)
{
    int n = 0;
    for (; x; n++) {
        x &= x - 1;
    }
    return n;
}

int popcount32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0f0f0f0fu;
    return (int) ((x * 0x01010101u) >> 24);
}

} // namespace

RadioPagingFlexReceiver::RadioPagingFlexReceiver() :
    m_sampleRate(38400.0),
    m_syncPower(1.0),
    m_s2Symbols(0),
    m_s2Count(0),
    m_syncSymbols(0),
    m_ersFirst(0),
    m_ersLast(0),
    m_ersDetections(0),
    m_syncs(0),
    m_frames(0),
    m_frameInfoFailures(0),
    m_lastCycle(0),
    m_lastFrame(0),
    m_lastQuality(0.0f),
    m_fiwKnown(false),
    m_fiwIndex(0),
    m_fiwTime(0.0)
{
    setPolarities(true, true);
    create(m_sampleRate);
}

void RadioPagingFlexReceiver::create(double sampleRate)
{
    m_sampleRate = sampleRate;
    m_syncDemod.create(sampleRate, RadioPagingFlex::m_syncBaud, 2);
    reset();
}

void RadioPagingFlexReceiver::reset()
{
    m_syncDemod.reset();
    m_decoder.reset();
    m_levelsMeasured = false;
    m_messages.clear();
    m_dedup.reset();
    m_time = 0.0;
    m_recentPower.clear();
    m_syncs = 0;
    m_frames = 0;
    m_frameInfoFailures = 0;
    m_syncSymbols = 0;
    m_ersFirst = 0;
    m_ersLast = 0;
    m_ersDetections = 0;
    m_mode = RadioPagingFlex::Mode1600_2;
    m_inverted = false;
    m_lastQuality = 0.0f;
    m_fiwKnown = false;
    m_fiwIndex = 0;
    m_fiwTime = 0.0;
    backToHunting();
}

// keepHistory: the last bits stay in the sync register, for a sync 1 found false (its frame information word
// failed): the true one may already be on its way
void RadioPagingFlexReceiver::backToHunting(bool keepHistory)
{
    m_state = Hunting;
    if (!keepHistory)
    {
        m_regLo = 0;
        m_regHi = 0;
    }
    m_syncDemod.setLocked(false);
    m_syncDemod.setHoldLevels(false);
}

// A complete message whose content and message number are those of one of the last messages of the pager (address,
// polarity) is a duplicate: a network without numbering sends every message with N = 0 and R = 0, so its messages are
// told apart by content; a numbered message sent again with a new number is a new message to the pager. The decoder
// also marks the retransmissions of numbered messages (R = 0, the words of the stored copy). Fragments, incomplete
// messages and messages without content (tone only) are not remembered
RadioPagingFlexDecoder::Message RadioPagingFlexReceiver::takeMessage()
{
    RadioPagingFlexDecoder::Message message = m_messages.front();
    m_messages.pop_front();

    if (!message.m_isFragment && message.m_complete && !message.m_text.empty())
    {
        std::string content = std::to_string((int) message.m_type) + "|" + std::to_string(message.m_messageNumber) + "|"
            + message.m_text;
        std::vector<uint8_t> data(content.begin(), content.end());
        bool duplicate = m_dedup.check(message.m_inverted ? 1 : 0, RadioPagingFlex::addressText(message.m_capcode), data,
            (int) data.size() * 8, m_time);

        message.m_duplicate = message.m_duplicate || duplicate;
    }

    return message;
}

void RadioPagingFlexReceiver::flush()
{
    if (m_state == Data) {
        endFrame(true);
    }

    std::vector<RadioPagingFlexDecoder::Message> messages = m_decoder.flush();
    for (auto& message : messages) {
        m_messages.push_back(message);
    }
}

RadioPagingFlexReceiver::Status RadioPagingFlexReceiver::getStatus() const
{
    Status status;

    status.m_state = m_state;
    status.m_mode = m_mode;
    status.m_inverted = m_inverted;
    status.m_cycle = m_lastCycle;
    status.m_frame = m_lastFrame;
    status.m_syncs = m_syncs;
    status.m_frames = m_frames;
    status.m_frameInfoFailures = m_frameInfoFailures;
    status.m_quality = m_lastQuality;
    status.m_deviation = m_syncDemod.getDeviation();
    // In a frame the data demodulator has the levels (it tracks them on 2-level data)
    bool inFrame = (m_state == Sync2) || (m_state == Data);
    status.m_dc = inFrame ? m_dataDemod.getDC() : m_syncDemod.getDC();
    // BER and frame report of the last frame; network state of its polarity
    status.m_ber = m_decoder.status().m_ber;

    for (int p = 0; p < 4; p++) {
        status.m_phaseBer[p] = m_decoder.status().m_phaseBer[p];
    }
    status.m_frameReport = m_decoder.lastFrame();

    for (int pol = 0; pol < 2; pol++)
    {
        status.m_frameReportPol[pol] = m_decoder.lastFrame(pol == 1);
        status.m_berPol[pol] = m_decoder.ber(pol == 1);

        for (int p = 0; p < 4; p++) {
            status.m_phaseBerPol[pol][p] = m_decoder.phaseBer(pol == 1, p);
        }
    }

    status.m_networkTime = m_decoder.networkTime(m_inverted);
    status.m_groups = m_decoder.groups(m_inverted);
    status.m_reassemblies = m_decoder.reassemblies(m_inverted);
    status.m_ers = ersActive();
    status.m_ersDetections = m_ersDetections;
    status.m_ersSeconds = (m_ersDetections > 0) ? (float) (m_ersLast - m_ersFirst) / RadioPagingFlex::m_syncBaud : 0.0f;

    return status;
}

// The last 96 bits are a cycle of the re-synchronization, starting at either half
bool RadioPagingFlexReceiver::checkErs() const
{
    uint64_t newer = m_regLo & mask48;
    uint64_t older = (((m_regHi & 0xffffffffULL) << 16) | (m_regLo >> 48)) & mask48;
    uint64_t other = ~ersHalf & mask48;
    int errors = std::min(popcount64(older ^ ersHalf) + popcount64(newer ^ other),
        popcount64(older ^ other) + popcount64(newer ^ ersHalf));

    return errors <= m_maxErsErrors;
}

// A cycle matches every half cycle while it lasts: active until two halves are missed
bool RadioPagingFlexReceiver::ersActive() const
{
    return (m_ersDetections > 0) && (m_syncSymbols - m_ersLast <= (uint64_t) ersCycleBits);
}

void RadioPagingFlexReceiver::process(float sample, float magsq)
{
    RadioPagingFskDemod::Symbol symbol;

    m_time += 1.0 / m_sampleRate;

    // The sync demodulator keeps its symbol clock running through the data part, so the
    // next frame's sync is found without a new acquisition
    if (m_syncDemod.process(sample, symbol))
    {
        m_syncSymbols++;

        if ((m_state == Hunting) || (m_state == FrameInfo)) {
            syncSymbol(symbol, magsq);
        }
    }

    if ((m_state == Sync2) || (m_state == Data))
    {
        if (m_dataDemod.process(sample, symbol)) {
            dataSymbol(symbol.m_value, symbol.m_error, symbol.m_soft, symbol.m_centreValue, magsq);
        }
        if ((m_state == Data) && (m_dataCount < m_dataSymbols))
        {
            int block = std::min(m_bitIndex / (32 * RadioPagingFlex::m_wordsPerBlock), RadioPagingFlex::m_blocksPerFrame - 1);
            m_blockPower[block] += magsq;
            m_blockSamples[block]++;
        }
    }

    if (m_state == Hunting)
    {
        m_recentPower.push_back(magsq);
        size_t keep = (size_t) (syncBits * m_sampleRate / RadioPagingFlex::m_syncBaud);
        while (m_recentPower.size() > keep) {
            m_recentPower.pop_front();
        }
    }
}

// Known levels of A, B and inverted A of a mode, most significant bit first; a 1 is a positive
// deviation in normal polarity
void RadioPagingFlexReceiver::fitSync1(RadioPagingFlex::Mode mode, bool inverted)
{
    std::vector<float> known;
    auto append = [&](uint32_t word, int bits) {
        for (int i = bits - 1; i >= 0; i--) {
            known.push_back((((word >> i) & 1) != 0) != inverted ? 1.0f : -1.0f);
        }
    };

    append(codeA[(int) mode], 32);
    append(codeB, 16);
    append(codeAInv[(int) mode], 32);

    if (m_syncDemod.fitLevels(known))
    {
        m_levelsMeasured = true;
        m_measuredDc = m_syncDemod.getDC();
    }

    m_syncDemod.setHoldLevels(true);
}

bool RadioPagingFlexReceiver::takeLevelsMeasured(float& dc)
{
    if (!m_levelsMeasured) {
        return false;
    }

    m_levelsMeasured = false;
    dc = m_measuredDc;
    return true;
}

void RadioPagingFlexReceiver::offsetLevels(float delta)
{
    m_syncDemod.offsetLevels(delta);
    m_dataDemod.offsetLevels(delta);
}

bool RadioPagingFlexReceiver::checkSync(RadioPagingFlex::Mode& mode, bool& inverted) const
{
    // Bits 79-48: A, 47-32: B, 31-0: inverted A; bits 95-80: the end of bit sync 1, reversals
    uint32_t a = (uint32_t) (((m_regHi & 0xffff) << 16) | (m_regLo >> 48));
    uint32_t b = (uint32_t) ((m_regLo >> 32) & 0xffff);
    uint32_t aInv = (uint32_t) m_regLo;
    uint32_t bitSync = (uint32_t) ((m_regHi >> 16) & 0xffff);
    int bitSyncErrors = std::min(popcount32(bitSync ^ 0xaaaau), popcount32(bitSync ^ 0x5555u));
    int best = syncBits + 1;
    int bestMode = -1;
    bool bestInverted = false;

    // Both polarities are scored, so the sync of a disabled one is rejected rather than
    // taken for the other
    for (int pol = 0; pol < 2; pol++)
    {
        uint32_t flip = pol ? 0xffffffffu : 0;
        int bErrors = popcount32((b ^ (flip & 0xffff)) ^ codeB);

        for (int m = 0; m < 4; m++)
        {
            int aErrors = popcount32((a ^ flip) ^ codeA[m]);
            int aInvErrors = popcount32((aInv ^ flip) ^ codeAInv[m]);
            int errors = bErrors + aErrors + aInvErrors;
            // A and inverted A are two copies of the same codeword. Within 2 bit errors a
            // copy is the codeword the code corrects it to; one copy may then have one more
            // error when the other confirms the same mode. B is not needed in that case
            bool copiesAgree = ((aErrors <= 2) && (aInvErrors <= 3)) || ((aErrors <= 3) && (aInvErrors <= 2));
            // Sync 1 follows bit sync 1: a sync with bit errors must have the reversals before A. Without this
            // the reversals and A of a sync 1 to come pass as an inverted B and inverted A, 48 bits early
            bool accepted = ((errors <= m_maxSyncErrors) || copiesAgree) && ((errors == 0) || (bitSyncErrors <= m_maxBitSyncErrors));

            if (accepted && (errors < best))
            {
                best = errors;
                bestMode = m;
                bestInverted = (pol == 1);
            }
        }
    }

    if ((bestMode < 0) || !m_enabled[bestInverted ? 1 : 0]) {
        return false;
    }

    mode = (RadioPagingFlex::Mode) bestMode;
    inverted = bestInverted;
    return true;
}

void RadioPagingFlexReceiver::syncSymbol(const RadioPagingFskDemod::Symbol& symbol, float magsq)
{
    (void) magsq;

    if (m_state == Hunting)
    {
        m_regHi = (m_regHi << 1) | (m_regLo >> 63);
        m_regLo = (m_regLo << 1) | (uint64_t) (symbol.m_value & 1);

        RadioPagingFlex::Mode mode;
        bool inverted;
        bool sync = checkSync(mode, inverted);

        // Sync 1 is closer to the re-synchronization than random bits: not checked on it
        if (!sync && (m_enabled[0] || m_enabled[1]) && checkErs())
        {
            // A new one after a gap of more than ersGapBits; it started a cycle ago
            if ((m_ersDetections == 0) || (m_syncSymbols - m_ersLast > (uint64_t) ersGapBits))
            {
                m_ersDetections++;
                m_ersFirst = m_syncSymbols - std::min<uint64_t>(m_syncSymbols, ersCycleBits);
            }
            m_ersLast = m_syncSymbols;
        }

        if (sync)
        {
            m_mode = mode;
            m_inverted = inverted;
            m_fiw = 0;
            m_fiwBits = 0;
            m_syncs++;
            m_state = FrameInfo;
            m_syncDemod.setLocked(true);
            // DC offset and deviation are measured on A, B and inverted A, the last 80
            // symbols, and held through the frame (the sync demodulator then sees symbols of
            // the frame's rate and levels) until it hunts again
            fitSync1(mode, inverted);

            double sum = 0.0;
            for (float p : m_recentPower) {
                sum += p;
            }
            m_syncPower = m_recentPower.empty() ? 1.0 : sum / m_recentPower.size();
        }
    }
    else if (m_state == FrameInfo)
    {
        int bit = mirror(symbol.m_value & 1, 2);
        m_fiw = (m_fiw << 1) | (uint32_t) bit;
        // The sync register goes on: if this sync 1 is false, the true one may be in these bits
        m_regHi = (m_regHi << 1) | (m_regLo >> 63);
        m_regLo = (m_regLo << 1) | (uint64_t) (symbol.m_value & 1);

        if (++m_fiwBits == 32)
        {
            RadioPagingBCH::Result result = RadioPagingBCH::decode(m_fiw);
            uint32_t corrected;

            // Three or more errors: taken when the word it corrects to is the frame expected next
            if ((result.m_bitErrors < 0) && frameInfoInSequence(m_fiw, corrected))
            {
                m_fiw = corrected;
                result = RadioPagingBCH::decode(m_fiw);
            }

            uint32_t data = RadioPagingFlex::wordData(result.m_word);

            if ((result.m_bitErrors < 0) || !RadioPagingFlex::checksumValid(data) || (((data >> 4) & 0xf) > 14))
            {
                m_frameInfoFailures++;
                backToHunting(true);
            }
            else
            {
                m_fiwKnown = true;
                m_fiwIndex = ((data >> 4) & 0xf) * 128 + ((data >> 8) & 0x7f);
                m_fiwTime = m_time;
                startData(symbol.m_offset);
            }
        }
    }
}

// A frame information word whose parity is still wrong after the correction of two bits (three or more errors):
// the corrected word is taken when its checksum is valid and its cycle and frame are those of the frame expected
// at this time after the last frame decoded (frames every 1.875 s, STD-43A 3.1.1), within the last 128 frames
bool RadioPagingFlexReceiver::frameInfoInSequence(uint32_t raw, uint32_t& corrected) const
{
    if (!m_fiwKnown) {
        return false;
    }

    // The parity bit flipped: the correction of bits 31-1 is the same and the parity then holds
    RadioPagingBCH::Result result = RadioPagingBCH::decode(raw ^ 1u);

    if (result.m_bitErrors != 2) {
        return false;
    }

    const double elapsed = (m_time - m_fiwTime) / frameSeconds;
    const long frames = std::lround(elapsed);

    if ((frames < 1) || (frames > 128) || (std::fabs(elapsed - (double) frames) > 0.1)) {
        return false;
    }

    const uint32_t data = RadioPagingFlex::wordData(result.m_word);
    const unsigned cycle = (data >> 4) & 0xf;
    const unsigned index = cycle * 128 + ((data >> 8) & 0x7f);
    const unsigned expected = (m_fiwIndex + (unsigned) frames) % (15 * 128);

    if (!RadioPagingFlex::checksumValid(data) || (cycle > 14) || (index != expected)) {
        return false;
    }

    corrected = result.m_word;
    return true;
}

void RadioPagingFlexReceiver::startData(float offset)
{
    int baud = RadioPagingFlex::baud(m_mode);
    int levels = RadioPagingFlex::levels(m_mode);
    double samplesPerSymbol = m_sampleRate / baud;

    m_dataDemod.create(m_sampleRate, baud, levels);
    m_dataDemod.setLevels(m_syncDemod.getDC(), m_syncDemod.getDeviation());
    m_dataDemod.setLocked(true);
    // The levels measured on sync 1 are kept for the frame
    m_dataDemod.setHoldLevels(true);
    // The first symbol of the second sync ends one symbol after the end of the frame
    // information word. Its matched filter fills during that symbol
    m_dataDemod.startSymbols(samplesPerSymbol - offset);

    m_s2Symbols = baud * 25 / 1000;
    m_s2Count = 0;
    m_s2Reg = 0;
    m_s2Tail.clear();
    m_s2TailSoft.clear();
    m_s2TailCentre.clear();
    m_s2TailStart = m_s2Symbols - 2;
    m_s2ExactEnd = -1;

    m_dataSymbols = baud * 1760 / 1000;
    m_dataCount = 0;
    m_bitIndex = 0;
    m_secondOfPair = false;
    m_words.assign(RadioPagingFlex::phaseCount(m_mode), std::vector<uint32_t>(RadioPagingFlex::m_wordsPerFrame, 0));
    m_centreWords = m_words;
    m_reliability.assign(RadioPagingFlex::phaseCount(m_mode), std::vector<uint8_t>(32 * RadioPagingFlex::m_wordsPerFrame, 0));
    m_blockPower.assign(RadioPagingFlex::m_blocksPerFrame, 0.0);
    m_blockSamples.assign(RadioPagingFlex::m_blocksPerFrame, 0);
    m_errorSum = 0.0;
    m_errorCount = 0;
    m_recentError2 = 0.0;       // The frame starts on its sync: clean

    m_state = Sync2;
}

bool RadioPagingFlexReceiver::isReceivingFrame() const
{
    return ((m_state == Sync2) || (m_state == Data)) && (m_recentError2 < m_receivingErrorRms * m_receivingErrorRms);
}

void RadioPagingFlexReceiver::dataSymbol(int symbol, float error, float soft, int centre, float magsq)
{
    int levels = RadioPagingFlex::levels(m_mode);
    int sym = mirror(symbol, levels);
    int centreSym = mirror(centre, levels);

    if (m_state == Sync2)
    {
        sync2Symbol(sym, soft, centreSym);
        return;
    }

    m_errorSum += (double) error * error;
    m_errorCount++;
    m_recentError2 += ((double) error * error - m_recentError2) / 32.0;
    storeDataSymbol(sym, soft, centreSym, magsq);

    if (++m_dataCount >= m_dataSymbols) {
        endFrame();
    }
}

void RadioPagingFlexReceiver::sync2Symbol(int sym, float soft, int centre)
{
    int levels = RadioPagingFlex::levels(m_mode);

    if (levels == 4) {
        m_s2Reg = ((m_s2Reg << 2) | grayToDibit[sym & 3]) & 0xffff;
    } else {
        m_s2Reg = ((m_s2Reg << 1) | (unsigned) (sym & 1)) & 0xffff;
    }

    m_s2Count++;

    if ((m_s2Count >= m_s2TailStart) && (m_s2Tail.size() < 4)) {
        m_s2Tail.push_back((uint8_t) sym);
        m_s2TailSoft.push_back(soft);
        m_s2TailCentre.push_back((uint8_t) centre);
    }
    if ((m_s2ExactEnd < 0) && (m_s2Count >= m_s2Symbols - 1) && (m_s2Reg == syncCInv)) {
        m_s2ExactEnd = m_s2Count;
    }

    // Decide where the data starts, one symbol after the nominal end at the latest
    if (m_s2Count >= m_s2Symbols + 1)
    {
        int boundary = m_s2Symbols;

        if ((m_s2ExactEnd >= m_s2Symbols - 1) && (m_s2ExactEnd <= m_s2Symbols + 1)) {
            boundary = m_s2ExactEnd;
        }

        m_state = Data;

        // Symbols received after the boundary are data
        for (size_t k = 0; k < m_s2Tail.size(); k++)
        {
            int position = m_s2TailStart + (int) k;
            if (position > boundary)
            {
                storeDataSymbol(m_s2Tail[k], m_s2TailSoft[k], m_s2TailCentre[k], (float) m_syncPower);
                m_dataCount++;
            }
        }
    }
}

// soft: the normalised symbol value (outer levels +/-1). The reliability of a bit is the distance of the symbol from
// the threshold that decides it: 0 for 2-level symbols and the first bit of 4-level symbols, +/-2/3 for the second
void RadioPagingFlexReceiver::storeDataSymbol(int sym, float soft, int centre, float magsq)
{
    (void) magsq;

    int levels = RadioPagingFlex::levels(m_mode);
    int phases = RadioPagingFlex::phaseCount(m_mode);
    int totalBits = 32 * RadioPagingFlex::m_wordsPerFrame;

    if (m_bitIndex >= totalBits) {
        return;
    }

    int word = m_bitIndex >> 5;
    int bit = 31 - (m_bitIndex & 31);

    const float magnitude = std::fabs(soft);
    const uint8_t outer = (uint8_t) std::min(255.0f, 100.0f * magnitude);
    const uint8_t inner = (uint8_t) std::min(255.0f, 100.0f * std::fabs(magnitude - 2.0f / 3.0f));

    // The bit of each decision: the whole symbol (m_words) and its centre (m_centreWords)
    auto setBit = [&](int phase, unsigned value, unsigned centreValue, uint8_t reliability) {
        if (value) {
            m_words[phase][word] |= (1u << bit);
        }
        if (centreValue) {
            m_centreWords[phase][word] |= (1u << bit);
        }
        m_reliability[phase][m_bitIndex] = reliability;
    };

    if (phases == 1)
    {
        setBit(0, sym & 1, centre & 1, outer);
        m_bitIndex++;
    }
    else if (levels == 2)
    {
        // 3200/2: bits alternate between phase A and phase C
        setBit(m_secondOfPair ? 1 : 0, sym & 1, centre & 1, outer);
        if (m_secondOfPair) {
            m_bitIndex++;
        }
        m_secondOfPair = !m_secondOfPair;
    }
    else if (phases == 2)
    {
        // 3200/4: a symbol carries a bit of phase A and a bit of phase C
        unsigned dibit = grayToDibit[sym & 3];
        unsigned centreDibit = grayToDibit[centre & 3];
        setBit(0, dibit >> 1, centreDibit >> 1, outer);
        setBit(1, dibit & 1, centreDibit & 1, inner);
        m_bitIndex++;
    }
    else
    {
        // 6400/4: symbols alternate between (A, B) and (C, D)
        unsigned dibit = grayToDibit[sym & 3];
        unsigned centreDibit = grayToDibit[centre & 3];
        int first = m_secondOfPair ? 2 : 0;
        setBit(first, dibit >> 1, centreDibit >> 1, outer);
        setBit(first + 1, dibit & 1, centreDibit & 1, inner);
        if (m_secondOfPair) {
            m_bitIndex++;
        }
        m_secondOfPair = !m_secondOfPair;
    }
}

// partial: the input stopped during the data part; the blocks from the first one not received whole are erased (each
// codeword of a block has bits all over it)
void RadioPagingFlexReceiver::endFrame(bool partial)
{
    static const char names2[2] = {'A', 'C'};
    static const char names4[4] = {'A', 'B', 'C', 'D'};

    RadioPagingFlexDecoder::RxFrame frame;
    int phases = RadioPagingFlex::phaseCount(m_mode);
    double threshold = m_syncPower * std::pow(10.0, -m_erasureDb / 10.0);

    frame.m_mode = m_mode;
    frame.m_inverted = m_inverted;
    frame.m_hasFiw = true;
    frame.m_fiw = m_fiw;
    double rms = (m_errorCount > 0) ? std::sqrt(m_errorSum / m_errorCount) : 1.0;
    frame.m_quality = std::max(0.0, std::min(1.0, 1.0 - 1.5 * rms));

    std::vector<uint8_t> erased(RadioPagingFlex::m_wordsPerFrame, 0);
    const int blocksReceived = partial ? (m_bitIndex / (32 * RadioPagingFlex::m_wordsPerBlock)) : RadioPagingFlex::m_blocksPerFrame;

    for (int block = 0; block < RadioPagingFlex::m_blocksPerFrame; block++)
    {
        if ((block >= blocksReceived) || ((m_blockSamples[block] > 0) && ((m_blockPower[block] / m_blockSamples[block]) < threshold)))
        {
            for (int w = 0; w < RadioPagingFlex::m_wordsPerBlock; w++) {
                erased[block * RadioPagingFlex::m_wordsPerBlock + w] = 1;
            }
        }
    }

    for (int p = 0; p < phases; p++)
    {
        RadioPagingFlexDecoder::RxPhase phase;
        phase.m_name = (phases == 1) ? 'A' : (phases == 2) ? names2[p] : names4[p];
        phase.m_words = m_words[p];
        phase.m_centreWords = m_centreWords[p];
        phase.m_erased = erased;
        phase.m_reliability = m_reliability[p];
        frame.m_phases.push_back(phase);
    }

    std::vector<RadioPagingFlexDecoder::Message> messages = m_decoder.decodeFrame(frame);
    for (auto& message : messages) {
        m_messages.push_back(message);
    }

    uint32_t data = RadioPagingFlex::wordData(RadioPagingBCH::decode(m_fiw).m_word);
    m_lastCycle = (data >> 4) & 0xf;
    m_lastFrame = (data >> 8) & 0x7f;
    m_lastQuality = (float) frame.m_quality;
    m_frames++;

    backToHunting();
}
