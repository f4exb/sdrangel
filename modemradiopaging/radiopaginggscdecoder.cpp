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

#include "radiopaginggscdecoder.h"

#include <algorithm>
#include <cmath>

const int RadioPagingGscDecoder::m_minNonBatterySaverBits;
const int RadioPagingGscDecoder::m_nonBatterySaverEndBits;
const int RadioPagingGscDecoder::m_maxNonBatterySaverLockMisses;
const int RadioPagingGscDecoder::m_syncHits;
const int RadioPagingGscDecoder::m_maxPreambleWordsAfterLock;
const float RadioPagingGscDecoder::m_minPairAgreement = 0.5f;
const int RadioPagingGscDecoder::m_minDamagedBlockCodewords;
const int RadioPagingGscDecoder::m_maxCommaErrors;
const int RadioPagingGscDecoder::m_alertSymbols;
const int RadioPagingGscDecoder::m_voiceMessageStart;
const int RadioPagingGscDecoder::m_defaultVoiceTimeoutSeconds;
constexpr float RadioPagingGscDecoder::m_dataWordNoiseQuality;
constexpr float RadioPagingGscDecoder::m_cleanQuality;
constexpr float RadioPagingGscDecoder::m_poorQuality;

namespace {

const int codewordBits = 46;
const int pairBits = 121;           // Comma, word 1, separator bit, word 2
const int peekBits = 74;            // Comma and word 1
const int hitRing = 2 * codewordBits + 1;
const int maxTransitionsForData = 108;
const uint16_t startComplement = RadioPagingGsc::m_startCode ^ 0xfff;
// Last bits handed to the address search when the 1100 pattern ends: the misses that ended
// it and the 4 bits of the pattern test, with margin
const int nonBatterySaverHandover = RadioPagingGscDecoder::m_nonBatterySaverEndBits + 7;

bool isWord1(uint16_t data)
{
    return (RadioPagingGsc::word1Index(data) >= 0) || (RadioPagingGsc::word1Index(data ^ 0xfff) >= 0);
}

} // namespace

RadioPagingGscDecoder::RadioPagingGscDecoder(Polarity polarity) :
    m_polarity(polarity),
    m_nonBatterySaver(false),
    m_voiceTimeoutSymbols(m_defaultVoiceTimeoutSeconds * RadioPagingGsc::m_dataBitRate),
    m_lastQuality(1.0f)
{
    reset();
}

void RadioPagingGscDecoder::setVoiceTimeout(double seconds)
{
    m_voiceTimeoutSymbols = std::max(1, (int) (seconds * RadioPagingGsc::m_dataBitRate));
}

void RadioPagingGscDecoder::reset()
{
    m_state = Hunting;
    m_parse = SeekStart;
    m_lastQuality = 1.0f;
    m_reg = 0;
    m_regBits = 0;
    std::fill(m_softRing, m_softRing + codewordBits, 0.0f);
    m_softPos = 0;
    std::fill(m_hits, m_hits + hitRing, (int8_t) -1);
    m_hitPos = 0;
    resetPattern();
    clearBuffer();
    m_preamble = -1;
    m_batch = false;
    m_nbs = false;
    m_slot = 0;
    m_active = false;
    m_current = Message();
    m_currentBits.clear();
    m_currentLost.clear();
    m_currentCorrected.clear();
    m_blocks = 0;
    m_codewords = 0;
    m_codewordsCorrected = 0;
    m_codewordsUncorrectable = 0;
    m_transmissions = 0;
    m_ber.reset();
    m_messageCount = 0;
    m_messages.clear();
    m_known.clear();
    m_voiceMessage = Message();
    m_voiceSymbols = 0;
    m_voiceHold = 0;
}

void RadioPagingGscDecoder::resetPattern()
{
    m_patternMissHistory.reset();
    m_patternBits = 0;
    m_patternLocked = false;
    m_patternRun = 0;
    m_patternMisses = 0;
}

void RadioPagingGscDecoder::addKnown(const std::vector<uint8_t>& bits, size_t symbolsAgo)
{
    KnownPattern pattern;
    float sign = (m_polarity == Normal) ? 1.0f : -1.0f;

    for (uint8_t b : bits) {
        pattern.m_levels.push_back((b ? 1.0f : -1.0f) * sign);
    }

    pattern.m_symbolsAgo = symbolsAgo;
    m_known.push_back(pattern);
}

bool RadioPagingGscDecoder::takeKnownPattern(KnownPattern& pattern)
{
    if (m_known.empty()) {
        return false;
    }

    pattern = m_known.front();
    m_known.pop_front();
    return true;
}

void RadioPagingGscDecoder::clearBuffer()
{
    m_buf.clear();
    m_soft.clear();
    m_pos = 0;
    m_scan = 0;
    m_commaBlocks = 0;
}

RadioPagingGscDecoder::Status RadioPagingGscDecoder::getStatus() const
{
    Status status;

    status.m_state = m_state;
    status.m_polarity = m_polarity;
    status.m_codewords = m_codewords;
    status.m_codewordsCorrected = m_codewordsCorrected;
    status.m_codewordsUncorrectable = m_codewordsUncorrectable;
    status.m_transmissions = m_transmissions;
    status.m_messages = m_messageCount;
    status.m_ber = m_ber;

    return status;
}

RadioPagingGscDecoder::Message RadioPagingGscDecoder::takeMessage()
{
    Message message = m_messages.front();
    m_messages.pop_front();
    return message;
}

uint32_t RadioPagingGscDecoder::wordAt(int pos) const
{
    return RadioPagingGsc::resolveDuplicated(&m_soft[pos]);
}

// The two symbols of a 300 bit/s bit are equal, those of a comma alternate. A word is
// taken where the pairs agree: the sum of each pair keeps at least half of the energy of
// its symbols over the word. A comma then never decodes as a word
bool RadioPagingGscDecoder::isWord(int pos) const
{
    float agree = 0.0f;
    float total = 0.0f;

    for (int i = 0; i < 23; i++)
    {
        float a = m_soft[pos + 2 * i];
        float b = m_soft[pos + 2 * i + 1];
        agree += std::fabs(a + b);
        total += std::fabs(a) + std::fabs(b);
    }

    return (total > 0.0f) && (agree >= m_minPairAgreement * total);
}

// Where the signal turns poor in count symbols from pos: the first symbol after which the rest (at least minPoor
// symbols) has a quality of limit or more while the symbols before it (at least minClean) are clean. -1 if it does
// not. A signal poor all along (weak) has no such point
int RadioPagingGscDecoder::noiseOnset(int pos, int count, float limit, int minClean, int minPoor) const
{
    for (int p = minClean; p <= count - minPoor; p++)
    {
        if ((softQuality(pos + p, count - p, 1) >= limit) && (softQuality(pos, p, 1) < m_cleanQuality)) {
            return p;
        }
    }

    return -1;
}

// rms distance of count soft symbols (from pos, every step) from the levels +/-1
float RadioPagingGscDecoder::softQuality(int pos, int count, int step) const
{
    float sum2 = 0.0f;

    for (int i = 0; i < count; i++)
    {
        float e = std::fabs(m_soft[pos + i * step]) - 1.0f;
        sum2 += e * e;
    }

    return std::sqrt(sum2 / (float) count);
}

bool RadioPagingGscDecoder::decodeWord(int pos, RadioPagingGsc::GolayResult& result) const
{
    if (!isWord(pos)) {
        return false;
    }

    result = RadioPagingGsc::golayDecode(wordAt(pos));
    return true;
}

float RadioPagingGscDecoder::correlation(int pos, uint32_t codeword) const
{
    float c = 0.0f;

    for (int i = 0; i < 23; i++)
    {
        float s = m_soft[pos + 2 * i] + m_soft[pos + 2 * i + 1];
        c += ((codeword >> i) & 1) ? s : -s;
    }

    return c;
}

// Preamble hit for the last 46 symbols: index * 2, plus 1 if complemented. -1 if none
int RadioPagingGscDecoder::preambleHit() const
{
    float window[codewordBits];

    for (int i = 0; i < codewordBits; i++) {
        window[i] = m_softRing[(m_softPos + i) % codewordBits];
    }

    RadioPagingGsc::GolayResult result = RadioPagingGsc::golayDecode(RadioPagingGsc::resolveDuplicated(window));
    int index = RadioPagingGsc::preambleIndex(result.m_data);

    if (index >= 0) {
        return index * 2;
    }

    index = RadioPagingGsc::preambleIndex(result.m_data ^ 0xfff);

    return (index >= 0) ? (index * 2 + 1) : -1;
}

void RadioPagingGscDecoder::feed(int symbol)
{
    feedSoft(symbol ? 1.0f : -1.0f);
}

void RadioPagingGscDecoder::feedSoft(float soft)
{
    float s = (m_polarity == Normal) ? soft : -soft;
    int bit = (s > 0.0f) ? 1 : 0;

    m_reg = (m_reg << 1) | (uint64_t) bit;
    if (m_regBits < 64) {
        m_regBits++;
    }
    m_softRing[m_softPos] = s;
    m_softPos = (m_softPos + 1) % codewordBits;

    // A new preamble is only looked for when no start code search is going on
    bool enabled = (m_state == Hunting) || (m_state == InTransmission) || (m_state == Voice);
    int hit = (m_regBits >= codewordBits) ? preambleHit() : -1;

    m_hitPos = (m_hitPos + 1) % hitRing;
    m_hits[m_hitPos] = (int8_t) hit;

    if (enabled && (hit >= 0))
    {
        int p1 = (m_hitPos + hitRing - codewordBits) % hitRing;
        int p2 = (m_hitPos + hitRing - 2 * codewordBits) % hitRing;

        if ((m_hits[p1] == hit) && (m_hits[p2] == hit))
        {
            if (m_state == Voice) {
                endVoice(VoiceNewTransmission, RadioPagingGsc::m_commaBits + m_syncHits * codewordBits);
            } else if (m_state == InTransmission) {
                endTransmission(true);
            }
            startTransmission(hit >> 1, (hit & 1) != 0);

            // The last three codewords were the preamble value (complemented in batch mode)
            std::vector<uint8_t> bits;
            uint32_t word = RadioPagingGsc::golayEncode(RadioPagingGsc::preambleValue(hit >> 1));
            if (hit & 1) {
                word ^= 0x7fffff;
            }
            for (int k = 0; k < m_syncHits; k++) {
                RadioPagingGsc::appendDuplicated(bits, word);
            }
            addKnown(bits, 0);
            return;
        }
    }

    if (m_regBits >= 5)
    {
        int b0 = (int) (m_reg & 1);
        int b2 = (int) ((m_reg >> 2) & 1);
        int b4 = (int) ((m_reg >> 4) & 1);

        bool onPattern = (b0 == b4) && (b0 != b2);

        if (!m_patternLocked)
        {
            // Locked on the last m_minNonBatterySaverBits bits with few bit errors
            m_patternMissHistory <<= 1;
            m_patternMissHistory[0] = !onPattern;
            m_patternBits = std::min(m_patternBits + 1, m_minNonBatterySaverBits);

            if ((m_patternBits >= m_minNonBatterySaverBits) && ((int) m_patternMissHistory.count() <= m_maxNonBatterySaverLockMisses))
            {
                m_patternLocked = true;
                m_patternRun = m_minNonBatterySaverBits;
                m_patternMisses = 0;
            }
        }
        else if (onPattern)
        {
            m_patternRun++;
            m_patternMisses = 0;
        }
        else if (++m_patternMisses < m_nonBatterySaverEndBits)
        {
            // A bit error within the pattern
            m_patternRun++;
        }
        else
        {
            int run = m_patternRun;
            resetPattern();

            if (m_nonBatterySaver && enabled)
            {
                if (m_state == Voice) {
                    endVoice(VoiceNewTransmission, run + 4);
                } else if (m_state == InTransmission) {
                    endTransmission(true);
                }
                startNonBatterySaver();

                // The pattern ended within the last nonBatterySaverHandover - 4 bits: the
                // bits before those are the 1100 pattern
                std::vector<uint8_t> bits;
                for (int i = 63; i >= nonBatterySaverHandover; i--) {
                    bits.push_back((uint8_t) ((m_reg >> i) & 1));
                }
                addKnown(bits, nonBatterySaverHandover);

                process();
                return;
            }
        }
    }

    if (m_state != Hunting)
    {
        m_buf.push_back((uint8_t) bit);
        m_soft.push_back(s);

        if (m_state == Voice) {
            processVoice();
        } else {
            process();
        }
    }
}

void RadioPagingGscDecoder::flush()
{
    if (m_state == Voice) {
        endVoice(VoiceSignalLost, 0);
    } else if (m_state != Hunting) {
        endTransmission(true);
    }
}

// The voice page has been reported: what follows is the alert time and the voice message
void RadioPagingGscDecoder::startVoice()
{
    m_voiceMessage = m_messages.back();
    m_voiceSymbols = 0;
    m_voiceHold = 0;
    m_state = Voice;
    clearBuffer();
}

// One symbol of the voice state. The last 121 symbols are tested for a comma and a pair of
// words: the turn-off word (activation code on Word 1 or Word 2) or, in the alert time, an
// interleaved tone only address. The comma has to be there (at most m_maxCommaErrors), so
// that voice audio is not taken for a word
void RadioPagingGscDecoder::processVoice()
{
    m_voiceSymbols++;

    if (m_voiceSymbols > m_voiceTimeoutSymbols)
    {
        endVoice(VoiceTimeout, 0);
        return;
    }

    int n = (int) m_buf.size();

    if (n >= 4096)
    {
        m_buf.erase(m_buf.begin(), m_buf.end() - pairBits);
        m_soft.erase(m_soft.begin(), m_soft.end() - pairBits);
        n = pairBits;
    }

    if (m_voiceHold > 0)
    {
        m_voiceHold--;
        return;
    }
    if ((n < pairBits) || (commaErrors(&m_buf[n - pairBits], RadioPagingGsc::m_commaBits) > m_maxCommaErrors)) {
        return;
    }

    int p = n - pairBits;
    RadioPagingGsc::GolayResult w1, w2;
    bool word1 = decodeWord(p + RadioPagingGsc::m_commaBits, w1);
    bool word2 = decodeWord(p + RadioPagingGsc::m_commaBits + codewordBits + 1, w2);

    if ((word1 && (w1.m_data == RadioPagingGsc::m_activationCode))
        || (word2 && (w2.m_data == (RadioPagingGsc::m_activationCode ^ 0xfff))))
    {
        endVoice(VoiceTurnOff, pairBits);
        return;
    }

    Message message;
    int e1, e2;

    if ((m_voiceSymbols <= m_voiceMessageStart) && parseAddress(p, message, e1, e2))
    {
        message.m_type = RadioPagingGsc::Tone;
        message.m_interleaved = true;
        message.m_errorsCorrected = e1 + e2;
        m_codewords += 2;
        m_codewordsCorrected += (e1 > 0 ? 1 : 0) + (e2 > 0 ? 1 : 0);
        m_ber.add(e1, 23, false);
        m_ber.add(e2, 23, false);
        m_messages.push_back(message);
        m_messageCount++;
        m_voiceHold = pairBits - 1;
    }
}

void RadioPagingGscDecoder::endVoice(VoiceEnd end, int before)
{
    Message update = m_voiceMessage;

    update.m_update = true;
    update.m_voiceEnd = end;
    update.m_voiceSeconds = (float) std::max(0, m_voiceSymbols - before) / (float) RadioPagingGsc::m_dataBitRate;
    m_messages.push_back(update);

    m_state = Hunting;
    clearBuffer();
}

void RadioPagingGscDecoder::startTransmission(int index, bool batch)
{
    std::fill(m_hits, m_hits + hitRing, (int8_t) -1);
    resetPattern();
    m_state = AwaitStart;
    m_parse = SeekStart;
    clearBuffer();
    m_preamble = index;
    m_batch = batch;
    m_nbs = false;
    m_slot = 0;
    m_active = false;
    m_transmissions++;
    m_ber.reset();
}

void RadioPagingGscDecoder::startNonBatterySaver()
{
    std::fill(m_hits, m_hits + hitRing, (int8_t) -1);
    resetPattern();
    m_state = AwaitStart;
    m_parse = SeekAddress;
    clearBuffer();

    // The pattern was taken as ended some bits into the comma
    for (int i = nonBatterySaverHandover - 1; i >= 0; i--)
    {
        m_buf.push_back((uint8_t) ((m_reg >> i) & 1));
        m_soft.push_back(m_softRing[(m_softPos + codewordBits - 1 - i) % codewordBits]);
    }

    m_preamble = -1;
    m_batch = false;
    m_nbs = true;
    m_slot = 0;
    m_active = false;
    m_transmissions++;
    m_ber.reset();
}

// Finishes the message in progress, if any, and returns to hunting.
// A message is truncated if the transmission ends before the message does
void RadioPagingGscDecoder::endTransmission(bool truncated)
{
    // An address with nothing after it is a complete tone only page: a tone only page is
    // its address alone (Rep. 900-2 Annex I 2.5 for non battery saver operation, Motorola
    // GSC manual Figure 6-2 for the individual and batch modes). A data page whose signal
    // ends before its first block cannot be told apart and is reported as tone only too
    if (m_active && (m_parse == Classify))
    {
        m_current.m_type = RadioPagingGsc::Tone;
        finishMessage(false);
    }
    else if (m_active)
    {
        finishMessage(truncated);
    }

    m_state = Hunting;
    clearBuffer();
}

void RadioPagingGscDecoder::finishMessage(bool truncated)
{
    m_current.m_truncated = truncated;
    m_current.m_rawBits = (int) m_currentBits.size();
    m_current.m_rawData.assign((m_currentBits.size() + 7) / 8, 0);
    m_current.m_rawLost.assign((m_currentBits.size() + 7) / 8, 0);
    m_current.m_rawCorrected.assign((m_currentBits.size() + 7) / 8, 0);

    for (size_t i = 0; i < m_currentBits.size(); i++)
    {
        if (m_currentBits[i]) {
            m_current.m_rawData[i >> 3] |= (uint8_t) (0x80 >> (i & 7));
        }
        if ((i < m_currentLost.size()) && m_currentLost[i]) {
            m_current.m_rawLost[i >> 3] |= (uint8_t) (0x80 >> (i & 7));
        }
        if ((i < m_currentCorrected.size()) && m_currentCorrected[i]) {
            m_current.m_rawCorrected[i >> 3] |= (uint8_t) (0x80 >> (i & 7));
        }
    }

    m_messages.push_back(m_current);
    m_messageCount++;
    m_currentBits.clear();
    m_currentLost.clear();
    m_currentCorrected.clear();
    m_active = false;
}

void RadioPagingGscDecoder::process()
{
    bool again = true;

    while (again && (m_state != Hunting))
    {
        switch (m_parse)
        {
        case SeekStart:
            again = processSeekStart();
            break;
        case SeekAddress:
            again = processSeekAddress();
            break;
        case Next:
            again = processNext();
            break;
        case Classify:
            again = processClassify();
            break;
        default:
            again = processData();
            break;
        }
    }

    trim();
}

void RadioPagingGscDecoder::trim()
{
    if ((m_state == InTransmission) && (m_pos >= 2048))
    {
        int n = std::min(m_pos, (int) m_buf.size());

        m_buf.erase(m_buf.begin(), m_buf.begin() + n);
        m_soft.erase(m_soft.begin(), m_soft.begin() + n);
        m_pos -= n;
    }
}

// The buffer starts at the end of the preamble word that locked. After each further
// preamble word (m_scan of them) a comma and the start code may follow. The start code is
// accepted when its Word 1 or its Word 2 decodes. Each copy is tried one symbol either
// side of the grid position and the position that correlates best is taken: it sets the
// symbol timing of the 600 bit/s blocks that follow
bool RadioPagingGscDecoder::processSeekStart()
{
    static const uint32_t word1 = RadioPagingGsc::golayEncode(RadioPagingGsc::m_startCode);
    static const uint32_t word2 = word1 ^ 0x7fffff;

    for (;;)
    {
        int grid = m_scan * codewordBits + RadioPagingGsc::m_commaBits;

        if (grid + 1 + 2 * codewordBits + 1 > (int) m_buf.size()) {
            return false;
        }

        bool found = false;
        float best = 0.0f;
        int bestPos = 0;

        for (int d = -1; d <= 1; d++)
        {
            int o = grid + d;
            RadioPagingGsc::GolayResult a, b;
            float c = -1.0e30f;

            if (decodeWord(o, a) && (a.m_data == RadioPagingGsc::m_startCode)) {
                c = correlation(o, word1);
            }
            if (decodeWord(o + codewordBits + 1, b) && (b.m_data == startComplement)) {
                c = std::max(c, correlation(o + codewordBits + 1, word2));
            }
            if ((c > -1.0e30f) && (!found || (c > best)))
            {
                found = true;
                best = c;
                bestPos = o;
            }
        }

        if (found)
        {
            m_pos = bestPos + 2 * codewordBits + 1;
            m_state = InTransmission;
            m_parse = Next;

            // Start code, separator bit and complement of the start code
            std::vector<uint8_t> bits;
            RadioPagingGsc::appendDuplicated(bits, word1);
            bits.push_back((uint8_t) ((word2 & 1) ^ 1));
            RadioPagingGsc::appendDuplicated(bits, word2);
            addKnown(bits, m_buf.size() - std::min(m_buf.size(), (size_t) m_pos));
            return true;
        }

        if (++m_scan > m_maxPreambleWordsAfterLock)
        {
            endTransmission(false);
            return false;
        }
    }
}

// First address after the pattern, at the bit offset where a comma and a word 1 are
bool RadioPagingGscDecoder::processSeekAddress()
{
    while (m_scan + pairBits <= (int) m_buf.size())
    {
        int o = m_scan;

        if (commaErrors(&m_buf[o], RadioPagingGsc::m_commaBits) <= m_maxCommaErrors)
        {
            Message message;
            int e1, e2;

            if (parseAddress(o, message, e1, e2))
            {
                m_pos = o;
                m_state = InTransmission;
                m_parse = Next;
                return true;
            }
        }

        if (++m_scan > 48)
        {
            endTransmission(false);
            return false;
        }
    }

    return false;
}

bool RadioPagingGscDecoder::parseAddress(int pos, Message& message, int& errors1, int& errors2) const
{
    RadioPagingGsc::GolayResult r1, r2;
    uint16_t word1;
    bool inverted1 = false;
    bool inverted2 = false;

    if (!decodeWord(pos + RadioPagingGsc::m_commaBits, r1) || !decodeWord(pos + RadioPagingGsc::m_commaBits + codewordBits + 1, r2)) {
        return false;
    }

    // The signal is not lost in the comma or the address: clean symbols that turn poor in (or before) a word that
    // needed two or three corrected bits are noise the code (perfect, every word decodes) corrected to the codeword of
    // another address. A word the signal ended in decodes with few corrections only by chance, a received one with
    // many corrections is rare
    const int addressSymbols = 2 * codewordBits + 1;
    const int symbols = RadioPagingGsc::m_commaBits + addressSymbols;
    const int word1End = RadioPagingGsc::m_commaBits + codewordBits;
    int poor = noiseOnset(pos, symbols, m_poorQuality, 16, 8);

    if ((poor >= 0) && (((poor < word1End) && (r1.m_bitErrors >= 2)) || ((poor < symbols) && (r2.m_bitErrors >= 2)))) {
        return false;
    }

    m_lastQuality = softQuality(pos + RadioPagingGsc::m_commaBits, addressSymbols, 1);

    if (RadioPagingGsc::word1Index(r1.m_data) >= 0)
    {
        word1 = r1.m_data;
    }
    else if (RadioPagingGsc::word1Index(r1.m_data ^ 0xfff) >= 0)
    {
        word1 = r1.m_data ^ 0xfff;
        inverted1 = true;
    }
    else
    {
        return false;
    }

    std::string address;

    if (!RadioPagingGsc::decodeAddress(word1, r2.m_data, m_preamble, address))
    {
        if (!RadioPagingGsc::decodeAddress(word1, r2.m_data ^ 0xfff, m_preamble, address)) {
            return false;
        }
        inverted2 = true;
    }

    message = Message();
    message.m_address = address;
    message.m_addressNumber = 1 + (inverted1 ? 2 : 0) + (inverted2 ? 1 : 0);
    message.m_preamble = m_preamble;
    message.m_slot = m_slot;
    message.m_batch = m_batch;
    message.m_nonBatterySaver = m_nbs;
    message.m_polarity = m_polarity;
    errors1 = r1.m_bitErrors;
    errors2 = r2.m_bitErrors;

    return true;
}

// After a message: another address, a repeated start code (Word 1 or Word 2), or the end
// of the transmission
bool RadioPagingGscDecoder::processNext()
{
    // The rest of a comma after a tone only address
    if (m_commaBlocks > 0)
    {
        if ((int) m_buf.size() < m_pos + RadioPagingGsc::m_blockBits) {
            return false;
        }
        if (isComma(&m_buf[m_pos]))
        {
            m_pos += RadioPagingGsc::m_blockBits;
            m_commaBlocks--;
            return true;
        }
        m_commaBlocks = 0;
    }

    if ((int) m_buf.size() < m_pos + peekBits) {
        return false;
    }

    RadioPagingGsc::GolayResult r, r2;
    bool w1 = decodeWord(m_pos + RadioPagingGsc::m_commaBits, r);

    Message message;
    int e1, e2;

    // A Word 1 that fails as an address may be a damaged start code Word 1: its Word 2 is
    // tested below
    if (w1 && isWord1(r.m_data))
    {
        if ((int) m_buf.size() < m_pos + pairBits) {
            return false;
        }
    }

    // The address after a message has its comma too (as the first one): a pair of words with many bits corrected
    // and no comma before them is what the end of a transmission (a carrier without modulation, noise) decodes to
    const bool comma = commaErrors(&m_buf[m_pos], RadioPagingGsc::m_commaBits) <= m_maxCommaErrors;

    if (w1 && isWord1(r.m_data) && parseAddress(m_pos, message, e1, e2) && (comma || (e1 + e2 <= m_maxErrorsWithoutComma)))
    {
        m_current = message;
        m_current.m_errorsCorrected = e1 + e2;
        m_currentBits.clear();
        m_currentLost.clear();
        m_currentCorrected.clear();
        m_active = true;
        m_slot++;
        m_codewords += 2;
        m_codewordsCorrected += (e1 > 0 ? 1 : 0) + (e2 > 0 ? 1 : 0);
        m_ber.add(e1, 23, false);
        m_ber.add(e2, 23, false);
        m_pos += pairBits;
        m_parse = Classify;
        return true;
    }

    if ((int) m_buf.size() < m_pos + pairBits) {
        return false;
    }

    bool w2 = decodeWord(m_pos + RadioPagingGsc::m_commaBits + codewordBits + 1, r2);

    if ((w1 && (r.m_data == RadioPagingGsc::m_startCode)) || (w2 && (r2.m_data == startComplement)))
    {
        m_pos += pairBits;
        return true;
    }

    endTransmission(false);
    return false;
}

// The block after an address tells the page type (Rep. 900-2 Annex I 2.3 and 2.6, Motorola
// GSC Figure 6-2):
//   activation code                              voice
//   data block (all codewords and checksum good) data
//   comma and a word (address, start code or a
//   preamble word of the next transmission)      tone only, the block is parsed next
//   alternating comma                            tone only; the comma is skipped as long
//                                                as it lasts, up to m_toneCommaBits
//   anything else                                a data block with errors if at least
//                                                m_minDamagedBlockCodewords codewords
//                                                decode, otherwise tone only (a first
//                                                block with more errors is lost: noise
//                                                after a tone only page looks the same)
bool RadioPagingGscDecoder::processClassify()
{
    if ((int) m_buf.size() < m_pos + RadioPagingGsc::m_blockBits) {
        return false;
    }

    const uint8_t *p = &m_buf[m_pos];

    RadioPagingGsc::GolayResult r;
    bool word = decodeWord(m_pos + RadioPagingGsc::m_commaBits, r);

    if (word && (r.m_data == RadioPagingGsc::m_activationCode))
    {
        m_current.m_type = RadioPagingGsc::Voice;
        finishMessage(false);
        startVoice();
        return false;
    }

    uint16_t codewords[8];
    uint8_t words[8];
    int good = 0;

    RadioPagingGsc::deinterleaveBlock(p + 1, codewords);

    for (int k = 0; k < 8; k++)
    {
        RadioPagingGsc::BchResult b = RadioPagingGsc::bchDecode(codewords[k]);

        if (b.m_bitErrors >= 0)
        {
            words[k] = b.m_data;
            good++;
        }
    }

    bool data = false;

    if (good == 8)
    {
        uint32_t sum = 0;

        for (int k = 0; k < 7; k++) {
            sum += RadioPagingGsc::bchEncode(words[k]);
        }
        data = (sum & 0x7f) == words[7];
    }

    if (!data && word && (commaErrors(p, RadioPagingGsc::m_commaBits) <= m_maxCommaErrors))
    {
        // Tone only, followed at once by an address, a start code or a preamble
        m_current.m_type = RadioPagingGsc::Tone;
        finishMessage(false);
        m_parse = Next;
        return true;
    }

    bool commaOnly = !data && isComma(p);

    if (!data && !commaOnly) {
        data = good >= m_minDamagedBlockCodewords;
    }

    if (data)
    {
        m_current.m_type = RadioPagingGsc::Data;
        m_blocks = 0;
        m_parse = Data;
    }
    else
    {
        m_current.m_type = RadioPagingGsc::Tone;
        finishMessage(false);

        // A comma after a tone only address is skipped block by block, as long as it lasts,
        // up to the m_toneCommaBits some transmitters send
        if (commaOnly)
        {
            m_pos += RadioPagingGsc::m_blockBits;
            m_commaBlocks = RadioPagingGsc::m_toneCommaBits / RadioPagingGsc::m_blockBits - 1;
        }
        m_parse = Next;
    }

    return true;
}

int RadioPagingGscDecoder::commaErrors(const uint8_t *bits, int n)
{
    int errors = 0;

    for (int i = 1; i < n; i++)
    {
        if (bits[i] == bits[i - 1]) {
            errors++;
        }
    }

    return errors;
}

// A block of 121 bits that is an alternating comma (commas fill unused batch slots,
// Rep. 900-2 Annex I 2.1)
bool RadioPagingGscDecoder::isComma(const uint8_t *bits)
{
    return (RadioPagingGsc::m_blockBits - 1 - commaErrors(bits, RadioPagingGsc::m_blockBits)) >= maxTransitionsForData;
}

bool RadioPagingGscDecoder::processData()
{
    if ((int) m_buf.size() < m_pos + RadioPagingGsc::m_blockBits) {
        return false;
    }

    uint16_t codewords[8];
    uint8_t words[8];
    bool lost[8] = {};
    bool corrected[8] = {};
    bool bad = false;

    RadioPagingGsc::deinterleaveBlock(&m_buf[m_pos + 1], codewords);

    // The signal lost in the block (clean symbols, then noise) or before it (the block as poor as noise after clean
    // symbols). Interleaved, a codeword has its bits all over the block: with more than two of them noise, the code
    // corrects it to some other codeword, and it is not taken
    const int blockSymbols = RadioPagingGsc::m_blockBits - 1;
    float quality = softQuality(m_pos + 1, blockSymbols, 1);
    int onset = noiseOnset(m_pos + 1, blockSymbols, m_dataWordNoiseQuality, 8, 8);
    bool lostBefore = (m_lastQuality < m_cleanQuality) && (quality >= m_dataWordNoiseQuality);
    m_lastQuality = quality;

    for (int k = 0; k < 8; k++)
    {
        RadioPagingGsc::BchResult b = RadioPagingGsc::bchDecode(codewords[k]);
        int noisy = 0;

        if (onset >= 0)
        {
            for (int j = 0; j < 15; j++) {
                noisy += (j * 8 + k >= onset) ? 1 : 0;
            }
        }
        if ((noisy > 2) || lostBefore) {
            b.m_bitErrors = -1;
        }

        m_codewords++;
        m_ber.add(b.m_bitErrors > 0 ? b.m_bitErrors : 0, 15, b.m_bitErrors < 0);
        if (b.m_bitErrors < 0)
        {
            words[k] = 0;
            lost[k] = true;
            bad = true;
            m_current.m_errorsUncorrectable++;
            m_codewordsUncorrectable++;
        }
        else
        {
            words[k] = b.m_data;
            if (b.m_bitErrors > 0)
            {
                m_current.m_errorsCorrected += b.m_bitErrors;
                m_codewordsCorrected++;
                corrected[k] = true;
            }
        }
    }

    if (!bad)
    {
        uint32_t sum = 0;

        for (int k = 0; k < 7; k++) {
            sum += RadioPagingGsc::bchEncode(words[k]);
        }
        if ((sum & 0x7f) != words[7]) {
            m_current.m_checksumErrors++;
        } else {
            m_current.m_checksumPassed++;
        }
    }

    for (int k = 0; k < 6; k++)
    {
        for (int i = 0; i < 7; i++)
        {
            m_currentBits.push_back((words[k] >> i) & 1);
            m_currentLost.push_back(lost[k] ? 1 : 0);
            m_currentCorrected.push_back(corrected[k] ? 1 : 0);
        }
    }
    for (int i = 0; i < 6; i++)
    {
        m_currentBits.push_back((words[6] >> i) & 1);
        m_currentLost.push_back(lost[6] ? 1 : 0);
        m_currentCorrected.push_back(corrected[6] ? 1 : 0);
    }

    // The last bit of word 6 tells whether a block follows: lost, the end of the message is not known
    bool more = (words[6] & 0x40) != 0;

    m_pos += RadioPagingGsc::m_blockBits;
    m_blocks++;

    if (lost[6] || !more || (m_blocks >= RadioPagingGsc::m_maxBlocks))
    {
        finishMessage(more || lost[6]);
        m_parse = Next;
    }

    return true;
}
